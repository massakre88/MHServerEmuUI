#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#define APACHE_BIN "/Apache24/bin"
#define SERVER_EMU_DIR "/MHServerEmu"
#define LIVETUNING_DATA "/Data/Game/LiveTuningData.json"
#include <QMainWindow>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSlider>
#include <QLineEdit>
#include <QVBoxLayout>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onBrowseButtonClicked();  // Slot for the Browse button
    void onStartClientButtonClicked();
    void onUpdateButtonClicked();
    void onDownloadFinished(QNetworkReply *reply);
    void startServer();
    void stopServer();
    void readServerOutput();
    void handleServerError();
    void onLoadLiveTuning();       // Load Live Tuning values
    void onSaveLiveTuning();       // Save Live Tuning values
    void onReloadLiveTuning();     // Reload command for server
    void onPushButtonShutdownClicked(); // Slot for server shutdown
    void onCategoryChanged(const QString &category);
    void onPushButtonAddLTSettingClicked();
    void onPushButtonLoadConfigClicked();
    void onPushButtonSaveConfigClicked();
    void onUpdateLevelButtonClicked();
    void onPushButtonBanClicked();
    void onPushButtonUnBanClicked();
    void onKickButtonClicked();
    void onServerPathEditUpdated();
    void onPushButtonSendToServerClicked();

private:
    Ui::MainWindow *ui;
    QProcess *apacheProcess;
    QProcess *serverProcess;
    int playerCount;               // Tracks the number of logged-in players
    void updatePlayerCountLabel(); // Updates the player count label
    QVBoxLayout *liveTuningLayout; // Layout to hold sliders dynamically
    QString liveTuningFilePath;    // Path to LiveTuningData.json
    void createLiveTuningSliders(const QJsonArray &data); // Create sliders from JSON
    QMap<QString, QJsonArray> categories;  // Holds categorized data
    void populateComboBox();
    void displayCategoryItems(const QString &category);
    void clearLayout(QLayout *layout);
    QPixmap onPixmap;  // Image for the "on" state
    QPixmap offPixmap; // Image for the "off" state
    QTimer *statusCheckTimer; // Timer to periodically check status
    void updateServerStatus();    
};

#endif // MAINWINDOW_H
