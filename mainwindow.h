#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

class QTextEdit;
class QComboBox;
class QPushButton;
class QLineEdit;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateConversion();      // Met à jour la zone "Sortie ASCII"
    void updateTransformation();  // Met à jour la zone "Transformation formattée"
    void mappingSelectionChanged(int index);
    void syncSelectionFromInput();
    void syncSelectionFromOutput();
    void onCopierClicked();
    void onSupprimerLignesIdentiquesClicked(); // Nouveau slot

private:
    bool loadMapping(const QString &filename);
    QString convertString(const QString &input,
                          const QMap<QChar, QString> &mapping,
                          const QString &defaultReplacement);
    QString processRepeatingTemplate(const QString &tpl,
                                     const QString &original,
                                     const QString &converted);
    QString processGlobalTemplate(const QString &tpl);

    // Widgets de l'interface
    QTextEdit  *textEditInput;         // Entrée UTF‑8
    QTextEdit  *textEditOutput;        // Conversion ASCII pure
    QComboBox  *comboBoxMapping;       // Sélection du mapping
    QLineEdit  *lineEditFormat;         // Zone de saisie du template (une seule ligne)
    QTextEdit  *textEditTransformed;   // Résultat de la transformation
    QPushButton *buttonCopier;         // Bouton "Copier"
    QPushButton *buttonSupprimer;      // Bouton "Supprimer les lignes identiques"

    // Données de mapping
    QMap<QChar, QString> mapping;
    QString defaultReplacement;
    QString mappingDirPath; // Chemin vers le dossier "mappings"

    // Pour éviter les mises à jour récursives lors de la synchronisation
    bool syncInProgress;
};

#endif // MAINWINDOW_H
