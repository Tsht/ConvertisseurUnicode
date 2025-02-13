#include "mainwindow.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QRegularExpression>
#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QTextCursor>
#include <QTextBlock>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    defaultReplacement("?"),
    syncInProgress(false)
{
    // Définir le chemin du dossier des mappings (situé à côté de l'exécutable)
    mappingDirPath = QCoreApplication::applicationDirPath() + "/mappings";

    // Création du widget central et du layout principal
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // --- Ligne supérieure : Entrée UTF‑8 et Sortie ASCII ---
    QHBoxLayout *topLayout = new QHBoxLayout;

    // Groupe d'entrée UTF‑8
    QGroupBox *inputGroup = new QGroupBox(tr("Entrée UTF‑8"), this);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    textEditInput = new QTextEdit(this);
    inputLayout->addWidget(textEditInput);
    topLayout->addWidget(inputGroup);

    // Groupe de sortie ASCII (conversion pure)
    QGroupBox *outputGroup = new QGroupBox(tr("Sortie ASCII"), this);
    QVBoxLayout *outputLayout = new QVBoxLayout(outputGroup);
    textEditOutput = new QTextEdit(this);
    textEditOutput->setReadOnly(true);
    outputLayout->addWidget(textEditOutput);
    topLayout->addWidget(outputGroup);

    mainLayout->addLayout(topLayout);

    // --- Ligne pour le template de format (une seule ligne) ---
    QHBoxLayout *formatLayout = new QHBoxLayout;
    QLabel *formatLabel = new QLabel(tr("Format :"), this);
    formatLayout->addWidget(formatLabel);
    lineEditFormat = new QLineEdit(this);
    // Exemple par défaut de template (une seule ligne)
    lineEditFormat->setText(tr("mv \"$1\" \"$2\""));
    formatLayout->addWidget(lineEditFormat);
    mainLayout->addLayout(formatLayout);

    // --- Zone de transformation formattée ---
    QGroupBox *transGroup = new QGroupBox(tr("Transformation formattée"), this);
    QVBoxLayout *transLayout = new QVBoxLayout(transGroup);
    textEditTransformed = new QTextEdit(this);
    textEditTransformed->setReadOnly(true);
    transLayout->addWidget(textEditTransformed);
    mainLayout->addWidget(transGroup);

    // --- Bouton "Copier" ---
    buttonCopier = new QPushButton(tr("Copier"), this);
    mainLayout->addWidget(buttonCopier);

    // --- Bouton "Supprimer les lignes identiques" ---
    buttonSupprimer = new QPushButton(tr("Supprimer les lignes identiques"), this);
    mainLayout->addWidget(buttonSupprimer);

    // --- Sélection du fichier de mapping ---
    QHBoxLayout *mappingSelLayout = new QHBoxLayout;
    QLabel *mappingSelLabel = new QLabel(tr("Fichier de mapping :"), this);
    comboBoxMapping = new QComboBox(this);
    mappingSelLayout->addWidget(mappingSelLabel);
    mappingSelLayout->addWidget(comboBoxMapping);
    mainLayout->addLayout(mappingSelLayout);

    // Vérification et création du dossier de mapping si nécessaire
    QDir mappingDir(mappingDirPath);
    if (!mappingDir.exists()) {
        if (mappingDir.mkpath(mappingDirPath)) {
            QMessageBox::information(this, tr("Information"),
                                     tr("Le dossier de mapping n'existait pas et a été créé :\n%1\nAucun mapping défini, la conversion par défaut va s'appliquer.")
                                         .arg(mappingDirPath));
        } else {
            QMessageBox::warning(this, tr("Erreur"),
                                 tr("Impossible de créer le dossier de mapping :\n%1").arg(mappingDirPath));
        }
        comboBoxMapping->addItem(tr("Aucun mapping défini"));
    } else {
        QStringList mapFiles = mappingDir.entryList(QStringList() << "*.map8u", QDir::Files);
        if (mapFiles.isEmpty()) {
            comboBoxMapping->addItem(tr("Aucun mapping défini"));
        } else {
            comboBoxMapping->addItems(mapFiles);
        }
    }

    // --- Connexions des signaux/slots ---
    connect(textEditInput, &QTextEdit::textChanged, this, [&](){
        updateConversion();
        updateTransformation();
    });
    connect(lineEditFormat, &QLineEdit::textChanged, this, &MainWindow::updateTransformation);
    connect(comboBoxMapping, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::mappingSelectionChanged);
    connect(buttonCopier, &QPushButton::clicked, this, &MainWindow::onCopierClicked);
    connect(buttonSupprimer, &QPushButton::clicked, this, &MainWindow::onSupprimerLignesIdentiquesClicked);
    connect(textEditInput, &QTextEdit::cursorPositionChanged, this, &MainWindow::syncSelectionFromInput);
    connect(textEditOutput, &QTextEdit::cursorPositionChanged, this, &MainWindow::syncSelectionFromOutput);

    // Chargement initial du mapping (si défini)
    if (comboBoxMapping->count() > 0) {
        if (comboBoxMapping->currentText() != tr("Aucun mapping défini"))
            loadMapping(comboBoxMapping->currentText());
        else {
            mapping.clear();
            defaultReplacement = "?";
        }
    }

    updateConversion();
    updateTransformation();
}

MainWindow::~MainWindow()
{
}

void MainWindow::mappingSelectionChanged(int index)
{
    Q_UNUSED(index);
    QString filename = comboBoxMapping->currentText();
    if (filename == tr("Aucun mapping défini")) {
        mapping.clear();
        defaultReplacement = "?";
        updateConversion();
        updateTransformation();
        return;
    }
    if (!filename.isEmpty()) {
        if (loadMapping(filename)) {
            updateConversion();
            updateTransformation();
        }
    }
}

bool MainWindow::loadMapping(const QString &filename)
{
    QString filePath = mappingDirPath + "/" + filename;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Erreur"),
                             tr("Impossible d'ouvrir le fichier de mapping :\n%1").arg(filePath));
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    QString content = QString::fromUtf8(data);
    QStringList lines = content.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    if (content.contains(QChar::ReplacementCharacter)) {
        QMessageBox::warning(this, tr("Erreur"),
                             tr("Le fichier de mapping n'est pas encodé en UTF‑8 :\n%1").arg(filePath));
        return false;
    }

    mapping.clear();
    defaultReplacement = "";
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith("#"))
            continue;
        if (trimmed.startsWith("default=")) {
            defaultReplacement = trimmed.mid(QString("default=").length());
        } else {
            int eqIndex = trimmed.indexOf('=');
            if (eqIndex > 0) {
                QString keyStr = trimmed.left(eqIndex);
                QString valueStr = trimmed.mid(eqIndex + 1);
                if (!keyStr.isEmpty()) {
                    QChar keyChar = keyStr.at(0);
                    mapping.insert(keyChar, valueStr);
                }
            }
        }
    }
    if (defaultReplacement.isEmpty())
        defaultReplacement = "?";
    return true;
}

QString MainWindow::convertString(const QString &input,
                                  const QMap<QChar, QString> &mapping,
                                  const QString &defaultReplacement)
{
    QString output;
    for (int i = 0; i < input.length(); i++) {
        QChar c = input.at(i);
        if (c.unicode() < 128)
            output.append(c);
        else {
            if (mapping.contains(c))
                output.append(mapping.value(c));
            else
                output.append(defaultReplacement);
        }
    }
    return output;
}

QString MainWindow::processRepeatingTemplate(const QString &tpl,
                                               const QString &original,
                                               const QString &converted)
{
    QString result;
    int pos = 0;
    while (pos < tpl.size()) {
        int index = tpl.indexOf('$', pos);
        if (index == -1) {
            result.append(tpl.mid(pos));
            break;
        }
        // Compter le nombre de backslashes consécutifs qui précèdent le '$'
        int bsCount = 0;
        int j = index - 1;
        while (j >= pos && tpl[j] == '\\') {
            bsCount++;
            j--;
        }
        // Ajouter le texte précédent, en retirant les backslashes utilisés pour l'échappement
        result.append(tpl.mid(pos, index - pos - bsCount));
        // Afficher (bsCount/2) backslashes dans la sortie
        int outputBackslashes = bsCount / 2;
        result.append(QString(outputBackslashes, '\\'));
        // Si le nombre de backslashes est impair, le '$' est échappé
        bool isEscaped = (bsCount % 2 == 1);
        // On récupère le placeholder "$1" ou "$2"
        QString placeholder = tpl.mid(index, 2);
        if (placeholder == "$1" || placeholder == "$2") {
            if (isEscaped) {
                // Si échappé, afficher littéralement le placeholder
                result.append(placeholder);
            } else {
                // Sinon, effectuer la substitution
                result.append(placeholder == "$1" ? original : converted);
            }
            pos = index + 2;
        } else {
            // Si ce n'est pas un placeholder reconnu, on ajoute simplement '$'
            result.append("$");
            pos = index + 1;
        }
    }
    return result;
}


QString MainWindow::processGlobalTemplate(const QString &tpl)
{
    QString result = tpl;
    result.replace("\\$1", "$1");
    result.replace("\\$2", "$2");
    return result;
}

void MainWindow::updateConversion()
{
    // Mise à jour de la zone "Sortie ASCII" avec la conversion pure de l'entrée
    QString inputText = textEditInput->toPlainText();
    QStringList inputLines = inputText.split('\n');
    QString result;
    for (const QString &line : inputLines) {
        QString asciiConverted = convertString(line, mapping, defaultReplacement);
        result.append(asciiConverted + "\n");
    }
    textEditOutput->setPlainText(result);
}

void MainWindow::updateTransformation()
{
    // La zone "Transformation formattée" applique le template (unique) sur chaque ligne d'entrée.
    // Puisque le format est saisi dans un QLineEdit, on prend directement son texte.
    QString formatTemplate = lineEditFormat->text().trimmed();
    if (formatTemplate.isEmpty())
        formatTemplate = "$2";

    QString inputText = textEditInput->toPlainText();
    QStringList inputLines = inputText.split('\n');

    QString result;
    for (const QString &line : inputLines) {
        QString asciiConverted = convertString(line, mapping, defaultReplacement);
        QString processedLine = processRepeatingTemplate(formatTemplate, line, asciiConverted);
        result.append(processedLine + "\n");
    }
    textEditTransformed->setPlainText(result);
}

void MainWindow::syncSelectionFromInput()
{
    if (syncInProgress)
        return;
    syncInProgress = true;

    QTextCursor inputCursor = textEditInput->textCursor();
    int selStart = inputCursor.selectionStart();
    int selEnd = inputCursor.selectionEnd();

    // Ne synchroniser que si la sélection n'est pas vide
    if (selStart == selEnd) {
        syncInProgress = false;
        return;
    }

    QTextDocument *docIn = textEditInput->document();
    QTextBlock startBlock = docIn->findBlock(selStart);
    QTextBlock endBlock   = docIn->findBlock(selEnd);
    int startBlockNumber = startBlock.blockNumber();
    int endBlockNumber   = endBlock.blockNumber();

    QTextDocument *docOut = textEditOutput->document();
    QTextBlock outStartBlock = docOut->findBlockByNumber(startBlockNumber);
    QTextBlock outEndBlock   = docOut->findBlockByNumber(endBlockNumber);
    if (outStartBlock.isValid() && outEndBlock.isValid()) {
        int outStartPos = outStartBlock.position();
        int outEndPos   = outEndBlock.position() + outEndBlock.length() - 1;
        QTextCursor outCursor(docOut);
        outCursor.setPosition(outStartPos);
        outCursor.setPosition(outEndPos, QTextCursor::KeepAnchor);
        textEditOutput->setTextCursor(outCursor);
    }

    syncInProgress = false;
}

void MainWindow::syncSelectionFromOutput()
{
    if (syncInProgress)
        return;
    syncInProgress = true;

    QTextCursor outputCursor = textEditOutput->textCursor();
    int selStart = outputCursor.selectionStart();
    int selEnd = outputCursor.selectionEnd();

    // Ne synchroniser que s'il y a une sélection non vide
    if (selStart == selEnd) {
        syncInProgress = false;
        return;
    }

    QTextDocument *docOut = textEditOutput->document();
    QTextBlock startBlock = docOut->findBlock(selStart);
    QTextBlock endBlock = docOut->findBlock(selEnd);
    int startBlockNumber = startBlock.blockNumber();
    int endBlockNumber = endBlock.blockNumber();

    int inputLineCount = textEditInput->document()->blockCount();
    if (startBlockNumber >= inputLineCount) {
        QTextCursor inCursor = textEditInput->textCursor();
        inCursor.clearSelection();
        textEditInput->setTextCursor(inCursor);
    } else {
        QTextDocument *docIn = textEditInput->document();
        QTextBlock inStartBlock = docIn->findBlockByNumber(startBlockNumber);
        QTextBlock inEndBlock = docIn->findBlockByNumber(endBlockNumber);
        if (inStartBlock.isValid() && inEndBlock.isValid()) {
            int inStartPos = inStartBlock.position();
            int inEndPos = inEndBlock.position() + inEndBlock.length() - 1;
            QTextCursor inCursor(docIn);
            inCursor.setPosition(inStartPos);
            inCursor.setPosition(inEndPos, QTextCursor::KeepAnchor);
            textEditInput->setTextCursor(inCursor);
        }
    }
    syncInProgress = false;
}

void MainWindow::onCopierClicked()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(textEditTransformed->toPlainText());
}

void MainWindow::onSupprimerLignesIdentiquesClicked()
{
    // Récupère le contenu de la zone d'entrée
    QString inputText = textEditInput->toPlainText();
    QStringList inputLines = inputText.split('\n');

    // On filtre les lignes pour ne conserver que celles où la conversion diffère de la ligne d'origine
    QStringList filteredLines;
    for (const QString &line : inputLines) {
        // Calcul de la conversion ASCII pour la ligne
        QString asciiConverted = convertString(line, mapping, defaultReplacement);
        if (line != asciiConverted) {
            filteredLines.append(line);
        }
        // Sinon, la ligne est identique et est supprimée des deux zones.
    }

    // Met à jour la zone d'entrée avec les lignes filtrées
    QString newInputText = filteredLines.join("\n");
    textEditInput->setPlainText(newInputText);

    // La mise à jour de la conversion et de la transformation est déclenchée par le signal textChanged de textEditInput.
}
