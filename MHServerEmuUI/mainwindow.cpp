#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDir>
#include <QSettings>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDate>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , apacheProcess(new QProcess(this))
    , serverProcess(new QProcess(this))
    , playerCount(0)
    , statusCheckTimer(new QTimer(this)) // Initialize the timer in the initializer list

{
    ui->setupUi(this);

    // Load status indicator images
    onPixmap = QPixmap(":/images/on");
    offPixmap = QPixmap(":/images/off");
    if (onPixmap.isNull()) {
        qDebug() << "Failed to load /images/on from resource file.";
    }
    if (offPixmap.isNull()) {
        qDebug() << "Failed to load /images/off from resource file.";
    }

    // Set initial status indicators
    ui->mhServerStatusLabel->setPixmap(offPixmap);
    ui->apacheServerStatusLabel->setPixmap(offPixmap);

    // Connect the timer to the update function
    connect(statusCheckTimer, &QTimer::timeout, this, &MainWindow::updateServerStatus);

    // Start the timer to check status every 1 second
    statusCheckTimer->start(1000);

    this->setStyleSheet(
    "QCombBox { background: white; border: 1px solid gray; }"
    "QLineEdit { background: white; border: 1px solid gray; }"
    "QLabel { background: transparent; color: black; }"
    );

    // Link liveTuningLayout
    liveTuningLayout = qobject_cast<QVBoxLayout *>(ui->liveTuningContainer->layout());
    if (!liveTuningLayout) {
        qDebug() << "Error: liveTuningContainer does not have a QVBoxLayout.";
        return;
    }

    // Other initializations
    QSettings settings("PTM", "MHServerEmuUI");
    QString savedPath = settings.value("serverPath", "").toString();
    if (!savedPath.isEmpty()) {
        ui->mhServerPathEdit->setText(savedPath);
    }

    connect(ui->pushButtonShutdown, &QPushButton::clicked, this, &MainWindow::onPushButtonShutdownClicked);
    connect(ui->loadLiveTuningButton, &QPushButton::clicked, this, &MainWindow::onLoadLiveTuning);
    connect(ui->saveLiveTuningButton, &QPushButton::clicked, this, &MainWindow::onSaveLiveTuning);
    connect(ui->reloadLiveTuningButton, &QPushButton::clicked, this, &MainWindow::onReloadLiveTuning);
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseButtonClicked);
    connect(ui->startClientButton, &QPushButton::clicked, this, &MainWindow::onStartClientButtonClicked);
    connect(ui->updateButton, &QPushButton::clicked, this, &MainWindow::onUpdateButtonClicked);
    connect(ui->startServerButton, &QPushButton::clicked, this, &MainWindow::startServer);
    connect(ui->stopServerButton, &QPushButton::clicked, this, &MainWindow::stopServer);
    connect(serverProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::readServerOutput);
    connect(serverProcess, &QProcess::readyReadStandardError, this, &MainWindow::readServerOutput);
    connect(serverProcess, &QProcess::errorOccurred, this, &MainWindow::handleServerError);
    connect(ui->comboBoxCategory, &QComboBox::currentTextChanged, this, &MainWindow::onCategoryChanged);
    connect(ui->pushButtonAddLTsetting, &QPushButton::clicked, this, &MainWindow::onPushButtonAddLTSettingClicked);
    connect(ui->pushButtonLoadConfig, &QPushButton::clicked, this, &MainWindow::onPushButtonLoadConfigClicked);
    connect(ui->pushButtonSaveConfig, &QPushButton::clicked, this, &MainWindow::onPushButtonSaveConfigClicked);
    connect(ui->updateLevelButton, &QPushButton::clicked, this, &MainWindow::onUpdateLevelButtonClicked);
    connect(ui->pushButtonBan, &QPushButton::clicked, this, &MainWindow::onPushButtonBanClicked);
    connect(ui->pushButtonUnBan, &QPushButton::clicked, this, &MainWindow::onPushButtonUnBanClicked);
    connect(ui->KickButton, &QPushButton::clicked, this, &MainWindow::onKickButtonClicked);
    connect(ui->mhServerPathEdit, &QLineEdit::editingFinished, this, &MainWindow::onServerPathEditUpdated);
    connect(ui->pushButtonSendToServer, &QPushButton::clicked, this, &MainWindow::onPushButtonSendToServerClicked);
}

MainWindow::~MainWindow() {
    if (serverProcess->state() == QProcess::Running) {
        serverProcess->terminate();
        serverProcess->waitForFinished();
    }
    delete serverProcess;

    if (apacheProcess->state() == QProcess::Running) {
        apacheProcess->terminate();
        apacheProcess->waitForFinished();
    }
    delete apacheProcess;

    delete ui;
}

void MainWindow::onBrowseButtonClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select MH Server Directory"),
                                                    QDir::homePath(),
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        ui->mhServerPathEdit->setText(dir); // Update the Line Edit with the selected path

        // Save the path to settings
        QSettings settings("PTM", "MHServerEmuUI");
        settings.setValue("serverPath", dir);
    }
}

void MainWindow::startServer() {
    QString serverPath = ui->mhServerPathEdit->text();

    if (serverPath.isEmpty()) {
        QMessageBox::critical(this, "Error", "Please specify the server directory.");
        return;
    }

    QString apachePath = serverPath + "/Apache24/bin/httpd.exe";
    QString mhServerPath = serverPath + "/MHServerEmu/MHServerEmu.exe";
    QString apacheRoot = serverPath + "/Apache24";

    // Ensure no existing instances of the server or Apache are running
    QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "httpd.exe");
    QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "MHServerEmu.exe");

    // Check if executables exist
    if (!QFile::exists(apachePath)) {
        QMessageBox::critical(this, "Error", QString("Apache executable (httpd.exe) not found at %1").arg(apachePath));
        return;
    }

    if (!QFile::exists(mhServerPath)) {
        QMessageBox::critical(this, "Error", QString("MHServerEmu executable not found at %1").arg(mhServerPath));
        return;
    }

    // Set environment variable for Apache
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("APACHE_SERVER_ROOT", apacheRoot);
    apacheProcess->setProcessEnvironment(env);

    // Start Apache server
    apacheProcess->setWorkingDirectory(serverPath + "/Apache24/bin");
    apacheProcess->start(apachePath);
    if (!apacheProcess->waitForStarted()) {
        QMessageBox::critical(this, "Error", "Failed to start Apache server. Check your configuration.");
        return;
    }

    // Start MHServerEmu
    serverProcess->setWorkingDirectory(serverPath + "/MHServerEmu");
    serverProcess->start(mhServerPath);
    if (!serverProcess->waitForStarted()) {
        QMessageBox::critical(this, "Error", "Failed to start MHServerEmu.");
        apacheProcess->kill(); // Stop Apache if MHServerEmu fails to start
        return;
    }

    // Log the successful server start
    ui->ServerOutputEdit->append("Server started successfully.");
}

void MainWindow::stopServer() {
    ui->ServerOutputEdit->append("Stopping server...");

    // Disconnect error handling temporarily
    disconnect(serverProcess, &QProcess::errorOccurred, this, &MainWindow::handleServerError);

    // Stop MHServerEmu process
    if (serverProcess->state() == QProcess::Running) {
        serverProcess->terminate();
        if (!serverProcess->waitForFinished(5000)) { // Wait up to 5 seconds
            serverProcess->kill(); // Force kill if not finished
        }
    }

    // Stop Apache process
    if (apacheProcess->state() == QProcess::Running) {
        apacheProcess->terminate();
        if (!apacheProcess->waitForFinished(5000)) { // Wait up to 5 seconds
            apacheProcess->kill(); // Force kill if not finished
        }
    }

    // Reconnect error handling
    connect(serverProcess, &QProcess::errorOccurred, this, &MainWindow::handleServerError);

    // Fallback: Ensure both processes are killed using taskkill
    QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "MHServerEmu.exe");
    QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "httpd.exe");

    ui->ServerOutputEdit->append("Server stopped.");
    playerCount = 0; // Reset player count
    updatePlayerCountLabel();
}

void MainWindow::onPushButtonShutdownClicked() {
    // Ensure the server is running
    if (serverProcess->state() != QProcess::Running) {
        QMessageBox::warning(this, "Error", "Server is not running.");
        return;
    }

    // Get the shutdown time from lineEditShutdownTime
    bool ok;
    int shutdownTime = ui->lineEditShutdownTime->text().toInt(&ok);
    if (!ok || shutdownTime <= 0) {
        QMessageBox::warning(this, "Error", "Invalid shutdown time specified.");
        return;
    }

    // Get the shutdown message from lineEditShutdownMessage
    QString shutdownMessage = ui->lineEditShutdownMessage->text();
    shutdownMessage = shutdownMessage.replace("SHUTDOWNTIMER", QString::number(shutdownTime));

    // Broadcast the initial shutdown message
    QString broadcastCommand = QString("!server broadcast %1\n").arg(shutdownMessage);
    serverProcess->write(broadcastCommand.toUtf8());
    ui->ServerOutputEdit->append("Sent broadcast message: " + shutdownMessage);

    // Update the playerShutdownCount label with the initial time
    ui->playerShutdownCount->setText(QString("%1").arg(shutdownTime));

    // Start the countdown timer
    QTimer *shutdownTimer = new QTimer(this);

    // Convert shutdown time to seconds
    int totalSeconds = shutdownTime * 60;

    connect(shutdownTimer, &QTimer::timeout, this, [=]() mutable {
        // Decrement totalSeconds each second
        totalSeconds--;

        // Calculate remaining minutes and seconds
        int minutesRemaining = totalSeconds / 60;

        // Update the label with the remaining time
        if (totalSeconds > 0) {
            ui->playerShutdownCount->setText(QString("%1").arg(minutesRemaining));
        }

        if (totalSeconds == 60) {
            // Send the "one minute left" message
            QString oneMinuteMessage = "One minute left until server shutdown. Log out now to save your data!";
            QString oneMinuteCommand = QString("!server broadcast %1\n").arg(oneMinuteMessage);
            serverProcess->write(oneMinuteCommand.toUtf8());
            ui->ServerOutputEdit->append("Sent broadcast message: " + oneMinuteMessage);
        }

        if (totalSeconds <= 0) {
            // Send the shutdown command and stop the timer
            serverProcess->write("!server shutdown\n");
            ui->ServerOutputEdit->append("Sent server shutdown command.");
            ui->playerShutdownCount->setText("Server shutdown in progress...");
            shutdownTimer->stop();
            shutdownTimer->deleteLater(); // Clean up the timer
        }
    });

    // Start the timer with 1-second intervals
    shutdownTimer->start(1000);

    ui->ServerOutputEdit->append(QString("Shutdown countdown started. Server will shut down in %1 minutes.").arg(shutdownTime));
}

void MainWindow::readServerOutput() {
    QByteArray output = serverProcess->readAllStandardOutput();
    QString outputText = QString::fromLocal8Bit(output);

    QByteArray errorOutput = serverProcess->readAllStandardError();
    QString errorText = QString::fromLocal8Bit(errorOutput);

    // Append output to the ServerOutputEdit
    if (!outputText.isEmpty()) {
        ui->ServerOutputEdit->append(outputText);
    }

    if (!errorText.isEmpty()) {
        ui->ServerOutputEdit->append("<Error>: " + errorText);
    }

    // Check for player connection or disconnection logs
    if (outputText.contains("[PlayerConnectionManager] Accepted and registered client")) {
        playerCount++; // Increment player count
    } else if (outputText.contains("[PlayerConnectionManager] Removed client")) {
        playerCount--; // Decrement player count
        if (playerCount < 0) playerCount = 0; // Ensure count doesn't go negative
    }

    if (outputText.contains("[ServerManager] Shutdown finished")) {
        QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "MHServerEmu.exe");
        playerCount = 0; // Reset player count
        updatePlayerCountLabel();
    }

    // Update the player count label
    updatePlayerCountLabel();
}

void MainWindow::updatePlayerCountLabel() {
    ui->playerCountLabel->setText(QString::number(playerCount));
}

void MainWindow::handleServerError() {
   // QMessageBox::critical(this, "Error", "An error occurred in the server process.");
}

void MainWindow::onStartClientButtonClicked() {
    QString serverPath = ui->mhServerPathEdit->text(); // Get the directory path
    if (!serverPath.isEmpty()) {
        QString batFilePath = serverPath + "/StartClient.bat"; // Full path to the .bat file
        QProcess *process = new QProcess(this);
        process->setWorkingDirectory(serverPath); // Set the working directory
        process->startDetached("cmd.exe", {"/C", batFilePath}); // Pass the full .bat path
    }
}

void MainWindow::onUpdateButtonClicked() {
    // Check if MHServerEmu.exe or httpd.exe is running
    QStringList processesToCheck = {"MHServerEmu.exe", "httpd.exe"};
    QStringList runningProcesses;

    for (const QString &process : processesToCheck) {
        QProcess checkProcess;
        checkProcess.start("tasklist", {"/FI", QString("IMAGENAME eq %1").arg(process)});
        checkProcess.waitForFinished();

        QString output = checkProcess.readAllStandardOutput();
        if (output.contains(process, Qt::CaseInsensitive)) {
            runningProcesses.append(process);
        }
    }

    // If any processes are running, notify the user and stop the update
    if (!runningProcesses.isEmpty()) {
        QString message = "The following server processes are running:\n";
        message += runningProcesses.join("\n");
        message += "\n\nPlease stop the server before updating.";
        QMessageBox::warning(this, "Server Running", message);
        return;
    }

    // Define the nightly build time in GMT+0
    QTime nightlyBuildTime(7, 15, 0); // 7:15 AM GMT+0

    // Get the current UTC time
    QDateTime nowUTC = QDateTime::currentDateTimeUtc();

    // Calculate the correct date for the nightly build
    QDate nightlyDate = nowUTC.date();
    if (nowUTC.time() < nightlyBuildTime) {
        nightlyDate = nightlyDate.addDays(-1); // Use the previous day's date if before build time
    }

    // Get the nightly date in YYYYMMDD format
    QString currentDate = nightlyDate.toString("yyyyMMdd");

    // Construct the download URL with the correct nightly date
    QString downloadUrl = QString("https://nightly.link/Crypto137/MHServerEmu/workflows/nightly-release-windows-x64/master/MHServerEmu-nightly-%1-Release-windows-x64.zip").arg(currentDate);

    // Get the server path for saving the file
    QString serverPath = ui->mhServerPathEdit->text();
    if (serverPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Server path is empty. Set the path first.");
        return;
    }

    QString zipFilePath = serverPath + "/MHServerEmu-nightly.zip";
    QString extractPath = serverPath + "/MHServerEmu";

    // Prepare backup for files to preserve
    QStringList filesToBackup;
    if (ui->checkBoxUpdateConfigFile->isChecked()) {
        filesToBackup.append("config.ini");
    }
    if (ui->checkBoxUpdateSaveLiveTuning->isChecked()) {
        filesToBackup.append("Data/Game/LiveTuningData.json");
    }

    QMap<QString, QString> backupFiles; // Map original file paths to backup paths
    for (const QString &file : filesToBackup) {
        QString originalPath = extractPath + "/" + file;
        QString backupPath = originalPath + ".bak";
        if (QFile::exists(originalPath)) {
            if (QFile::rename(originalPath, backupPath)) {
                backupFiles.insert(originalPath, backupPath);
                qDebug() << "Backed up file:" << originalPath << "to" << backupPath;
            } else {
                QMessageBox::critical(this, "Error", "Failed to back up file: " + originalPath);
                qDebug() << "Failed to back up file:" << originalPath;
                return;
            }
        }
    }

    // Start downloading the file
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, [=](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, "Error", "Failed to download update: " + reply->errorString());
            reply->deleteLater();
            return;
        }

        // Save the downloaded file
        QByteArray fileData = reply->readAll();
        if (fileData.isEmpty()) {
            QMessageBox::critical(this, "Error", "Downloaded file is empty.");
            qDebug() << "Downloaded file is empty.";
            reply->deleteLater();
            return;
        }

        QFile file(zipFilePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(fileData);
            file.close();
            qDebug() << "Downloaded file saved to:" << zipFilePath;
        } else {
            QMessageBox::critical(this, "Error", "Failed to save downloaded file: " + file.errorString());
            qDebug() << "Failed to save file:" << zipFilePath << "Error:" << file.errorString();
            reply->deleteLater();
            return;
        }

        // Ensure the ZIP file exists
        if (!QFile::exists(zipFilePath)) {
            QMessageBox::critical(this, "Error", "The ZIP file does not exist after saving.");
            qDebug() << "ZIP file does not exist at:" << zipFilePath;
            reply->deleteLater();
            return;
        }

        // Ensure the extraction path exists
        QDir dir(extractPath);
        if (!dir.exists() && !dir.mkpath(".")) {
            QMessageBox::critical(this, "Error", "Failed to create extraction directory: " + extractPath);
            qDebug() << "Failed to create directory:" << extractPath;
            reply->deleteLater();
            return;
        }

        // Directly run PowerShell with QProcess
        QStringList args = {
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-Command",
            QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force").arg(zipFilePath, extractPath)
        };

        QProcess *process = new QProcess(this);
        process->setProgram("powershell");
        process->setArguments(args);
        connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
            qDebug() << "PowerShell Output:" << process->readAllStandardOutput();
        });
        connect(process, &QProcess::readyReadStandardError, this, [=]() {
            qDebug() << "PowerShell Error:" << process->readAllStandardError();
        });
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
            if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                // Restore backup files
                for (auto it = backupFiles.begin(); it != backupFiles.end(); ++it) {
                    QString originalPath = it.key();
                    QString backupPath = it.value();
                    if (QFile::exists(originalPath)) {
                        QFile::remove(originalPath); // Delete new file
                    }
                    if (QFile::rename(backupPath, originalPath)) {
                        qDebug() << "Restored file:" << backupPath << "to" << originalPath;
                    } else {
                        QMessageBox::critical(this, "Error", "Failed to restore file: " + backupPath);
                        qDebug() << "Failed to restore file:" << backupPath;
                    }
                }

                QFile::remove(zipFilePath); // Clean up the ZIP file after successful extraction
                QMessageBox::information(this, "Update", "Update completed successfully.");
                qDebug() << "Extraction completed successfully. ZIP file deleted.";
            } else {
                QMessageBox::critical(this, "Error", QString("Failed to extract update files. Exit Code: %1").arg(exitCode));
                qDebug() << "PowerShell failed with Exit Code:" << exitCode;
            }
        });

        process->start();
        reply->deleteLater();
    });

    manager->get(QNetworkRequest(QUrl(downloadUrl)));
}

void MainWindow::onDownloadFinished(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Download failed:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    // Handle the successful download
    QByteArray fileData = reply->readAll();
    qDebug() << "Download succeeded. Data size:" << fileData.size();
    reply->deleteLater();
}

void MainWindow::onLoadLiveTuning()
{
    // Retrieve path from the line edit
    QString mhServerPath = ui->mhServerPathEdit->text();
    if (mhServerPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please specify the MH Server path.");
        return;
    }

    // Construct the full path to livetuningdata.json
    QString filePath = QDir(mhServerPath).filePath("MHServerEmu/Data/Game/LiveTuningData.json");

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", QString("Could not open %1").arg(filePath));
        return;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isArray()) {
        QMessageBox::warning(this, "Error", "Invalid JSON format in LiveTuningData.json");
        return;
    }

    QJsonArray jsonArray = doc.array();

    // Category rules based on Setting prefixes
    QMap<QString, QString> categoryRules = {
        {"eGTV_", "Global"},
        {"eWETV_", "World Entity"},
        {"ePTV_", "Powers"},
        {"eRTV_", "Regions"},
        {"eLTV_", "Loot"},
        {"eMTV_", "Mission"},
        {"eCTV_", "Condition"},
        {"ePETV_", "Public Events"}
    };

    // Categorize items dynamically
    categories.clear();  // Reset the categories
    for (const QJsonValue &value : jsonArray) {
        if (!value.isObject()) {
            qDebug() << "Skipping non-object JSON entry:" << value;
            continue;
        }

        QJsonObject obj = value.toObject();
        QString setting = obj["Setting"].toString();
        QString category = "Uncategorized";

        for (auto it = categoryRules.begin(); it != categoryRules.end(); ++it) {
            if (setting.startsWith(it.key())) {
                category = it.value();
                break;
            }
        }

        categories[category].append(obj);
    }

    populateComboBox();  // Populate categories in the dropdown
}

void MainWindow::populateComboBox()
{
    ui->comboBoxCategory->clear();  // Clear any existing items
    for (const QString &category : categories.keys()) {
        ui->comboBoxCategory->addItem(category);
    }

    if (!categories.isEmpty()) {
        onCategoryChanged(ui->comboBoxCategory->currentText());
    }
}

void MainWindow::onCategoryChanged(const QString &category)
{
    displayCategoryItems(category);
}

void MainWindow::displayCategoryItems(const QString &category)
{
    // Clear the existing layout
    clearLayout(liveTuningLayout);

    // Ensure the selected category exists
    if (!categories.contains(category))
        return;

    const QJsonArray &items = categories[category];
    for (const QJsonValue &value : items) {
        QJsonObject obj = value.toObject();
        QString setting = obj["Setting"].toString();
        QString prototype = obj["Prototype"].toString();
        double val = obj["Value"].toDouble();

        // Optional: Add prototype label if Prototype is not empty
        if (!prototype.isEmpty()) {
            QLabel *prototypeLabel = new QLabel(QString("Prototype: %1").arg(prototype), this);
            liveTuningLayout->addWidget(prototypeLabel);
        }

        // Create label for the setting
        QLabel *label = new QLabel(setting, this);

        // Create slider
        QSlider *slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(0, 200); // Range for 0.0 to 10.0 (scaled by 0.5)
        slider->setValue(static_cast<int>(val * 10)); // Scale value for slider

        // Create line edit
        QLineEdit *lineEdit = new QLineEdit(QString::number(val, 'f', 1), this);
        lineEdit->setAlignment(Qt::AlignCenter);

        // Sync slider and line edit
        connect(slider, &QSlider::valueChanged, [lineEdit](int sliderValue) {
            double scaledValue = sliderValue / 10.0;
            lineEdit->setText(QString::number(scaledValue, 'f', 1));
        });

        connect(lineEdit, &QLineEdit::editingFinished, [slider, lineEdit]() {
            double enteredValue = lineEdit->text().toDouble();
            slider->setValue(static_cast<int>(enteredValue * 10));
        });

        // Add to layout
        QHBoxLayout *rowLayout = new QHBoxLayout();
        rowLayout->addWidget(label);
        rowLayout->addWidget(slider);
        rowLayout->addWidget(lineEdit);

        liveTuningLayout->addLayout(rowLayout); // Add the row to the main layout
    }

    // Force the layout to update
    liveTuningLayout->invalidate();
}

void MainWindow::clearLayout(QLayout *layout)
{
    while (QLayoutItem *child = layout->takeAt(0)) {
        if (QWidget *widget = child->widget()) {
            qDebug() << "Deleting widget:" << widget;
            widget->deleteLater();
        } else if (QLayout *nestedLayout = child->layout()) {
            qDebug() << "Clearing nested layout.";
            clearLayout(nestedLayout);
        }
        delete child;
    }
    qDebug() << "Layout cleared.";
}

void MainWindow::createLiveTuningSliders(const QJsonArray &data) {
    // Clear existing sliders
    QLayoutItem *child;
    while ((child = liveTuningLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    for (const QJsonValue &value : data) {
        QJsonObject obj = value.toObject();
        QString setting = obj["Setting"].toString();
        double val = obj["Value"].toDouble();

        QLabel *label = new QLabel(setting, this);
        QSlider *slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(0, 200); // Scale by 0.5 for range 0.0 to 10.0
        slider->setValue(static_cast<int>(val * 10));
        QLineEdit *lineEdit = new QLineEdit(QString::number(val, 'f', 1), this);

        connect(slider, &QSlider::valueChanged, [lineEdit](int sliderValue) {
            double scaledValue = sliderValue / 10.0;
            lineEdit->setText(QString::number(scaledValue, 'f', 1));
        });

        connect(lineEdit, &QLineEdit::editingFinished, [slider, lineEdit]() {
            double enteredValue = lineEdit->text().toDouble();
            slider->setValue(static_cast<int>(enteredValue * 10));
        });

        QHBoxLayout *rowLayout = new QHBoxLayout();
        rowLayout->addWidget(label);
        rowLayout->addWidget(slider);
        rowLayout->addWidget(lineEdit);
        liveTuningLayout->addLayout(rowLayout);
    }
}

void MainWindow::onSaveLiveTuning() {
    // Ensure liveTuningFilePath is set
    if (liveTuningFilePath.isEmpty()) {
        liveTuningFilePath = ui->mhServerPathEdit->text() + "/MHServerEmu/Data/Game/LiveTuningData.json";
    }

    // Prepare JSON array to store the settings
    QJsonArray savedArray;

    // Iterate through all categories in the map
    for (auto it = categories.constBegin(); it != categories.constEnd(); ++it) {
        const QJsonArray &items = it.value();

        for (const QJsonValue &value : items) {
            QJsonObject obj = value.toObject();

            // Retrieve values
            QString setting = obj["Setting"].toString();
            QString prototype = obj["Prototype"].toString();
            double currentValue = obj["Value"].toDouble();

            // Check if the setting exists in the UI and get its updated value
            double updatedValue = currentValue; // Default to original value
            for (int i = 0; i < liveTuningLayout->count() - 1; ++i) {
                QHBoxLayout *rowLayout = qobject_cast<QHBoxLayout *>(liveTuningLayout->itemAt(i)->layout());
                if (!rowLayout) continue;

                QLabel *label = qobject_cast<QLabel *>(rowLayout->itemAt(0)->widget());
                QLineEdit *lineEdit = qobject_cast<QLineEdit *>(rowLayout->itemAt(2)->widget());

                if (label && lineEdit && label->text() == setting) {
                    updatedValue = lineEdit->text().toDouble(); // Get updated value from UI
                    break;
                }
            }

            // Construct updated JSON object
            QJsonObject updatedObj;
            updatedObj["Setting"] = setting;
            updatedObj["Prototype"] = prototype;
            updatedObj["Value"] = updatedValue;

            // Append the updated object to the JSON array
            savedArray.append(updatedObj);
        }
    }

    // Save the JSON array to the file
    QFile file(liveTuningFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Error", QString("Failed to save Live Tuning data to %1").arg(liveTuningFilePath));
        return;
    }

    QJsonDocument doc(savedArray);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    // Inform the user of the successful save
    QMessageBox::information(this, "Success", "Live Tuning data saved successfully.");
}

void MainWindow::onReloadLiveTuning() {
    if (serverProcess->state() == QProcess::Running) {
        serverProcess->write("!server reloadlivetuning\n");
    } else {
        QMessageBox::warning(this, "Error", "Server is not running.");
    }
}

void MainWindow::onPushButtonAddLTSettingClicked()
{
    // Get the file path for the live tuning data
    QString mhServerPath = ui->mhServerPathEdit->text();
    if (mhServerPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please specify the MH Server path.");
        return;
    }

    QString filePath = QDir(mhServerPath).filePath("MHServerEmu/Data/Game/LiveTuningData.json");

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", QString("Could not open %1").arg(filePath));
        return;
    }

    // Load the existing JSON data
    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isArray()) {
        QMessageBox::warning(this, "Error", "Invalid JSON format in LiveTuningData.json");
        return;
    }

    QJsonArray jsonArray = doc.array();

    // Get the inputs from the line edits
    QString prototype = ui->lineEditAddLTSettingProto->text().trimmed();
    QString setting = ui->lineEditAddLTsettingName->text().trimmed();
    QString valueStr = ui->lineEditAddLTsettingValue->text().trimmed();

    if (setting.isEmpty() || valueStr.isEmpty()) {
        QMessageBox::warning(this, "Error", "Setting and Value fields cannot be empty.");
        return;
    }

    bool valueOk;
    double value = valueStr.toDouble(&valueOk);
    if (!valueOk) {
        QMessageBox::warning(this, "Error", "Invalid value entered.");
        return;
    }

    // Create the new entry
    QJsonObject newSetting;
    newSetting["Prototype"] = prototype;
    newSetting["Setting"] = setting;
    newSetting["Value"] = value;

    // Add the new entry to the JSON array
    jsonArray.append(newSetting);

    // Write the updated JSON data back to the file
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Error", "Unable to save LiveTuningData.json");
        return;
    }

    QJsonDocument updatedDoc(jsonArray);
    file.write(updatedDoc.toJson());
    file.close();

    QMessageBox::information(this, "Success", "New setting added successfully!");

    // Optionally reload the live tuning data to reflect the changes in the UI
    onLoadLiveTuning();
}

void MainWindow::onPushButtonLoadConfigClicked()
{
    // Construct the path to config.ini
    QString mhServerPath = ui->mhServerPathEdit->text();
    if (mhServerPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please specify the MH Server path.");
        return;
    }

    QString filePath = QDir(mhServerPath).filePath("MHServerEmu/config.ini");
    QSettings settings(filePath, QSettings::IniFormat);

    if (!QFile::exists(filePath)) {
        QMessageBox::warning(this, "Error", QString("Config file not found at: %1").arg(filePath));
        return;
    }

    // Load Logging settings
    ui->checkBoxEnableLogging->setChecked(settings.value("Logging/EnableLogging", false).toBool());
    ui->checkBoxSynchronousMode->setChecked(settings.value("Logging/SynchronousMode", false).toBool());
    ui->checkBoxHideSensitiveInformation->setChecked(settings.value("Logging/HideSensitiveInformation", false).toBool());
    ui->checkBoxEnableConsole->setChecked(settings.value("Logging/EnableConsole", false).toBool());
    ui->checkBoxConsoleIncludeTimestamps->setChecked(settings.value("Logging/ConsoleIncludeTimestamps", false).toBool());
    ui->comboBoxConsoleMinLevel->setCurrentIndex(settings.value("Logging/ConsoleMinLevel", 0).toInt());
    ui->comboBoxConsoleMaxLevel->setCurrentIndex(settings.value("Logging/ConsoleMaxLevel", 5).toInt());
    ui->checkBoxEnableFile->setChecked(settings.value("Logging/EnableFile", false).toBool());
    ui->checkBoxFileIncludeTimestamps->setChecked(settings.value("Logging/FileIncludeTimestamps", false).toBool());
    ui->comboBoxFileMinLevel->setCurrentIndex(settings.value("Logging/FileMinLevel", 0).toInt());
    ui->comboBoxFileMaxLevel->setCurrentIndex(settings.value("Logging/FileMaxLevel", 5).toInt());

    // Load Frontend settings
    ui->lineEditBindIP->setText(settings.value("Frontend/BindIP", "").toString());
    ui->lineEditPort->setText(settings.value("Frontend/Port", "").toString());
    ui->lineEditPublicAddress->setText(settings.value("Frontend/PublicAddress", "").toString());
    ui->lineEditReceiveTimeoutMS->setText(settings.value("Frontend/ReceiveTimeoutMS", "").toString());
    ui->lineEditSendTimeoutMS->setText(settings.value("Frontend/SendTimeoutMS", "").toString());

    // Load Auth settings
    ui->lineEditAuthAddress->setText(settings.value("Auth/Address", "").toString());
    ui->lineEditAuthPort->setText(settings.value("Auth/Port", "").toString());
    ui->checkBoxEnableWebApi->setChecked(settings.value("Auth/EnableWebApi", false).toBool());

    // Load PlayerManager settings
    ui->checkBoxUseJsonDBManager->setChecked(settings.value("PlayerManager/UseJsonDBManager", false).toBool());
    ui->checkBoxIgnoreSessionToken->setChecked(settings.value("PlayerManager/IgnoreSessionToken", false).toBool());
    ui->checkBoxAllowClientVersionMismatch->setChecked(settings.value("PlayerManager/AllowClientVersionMismatch", false).toBool());
    ui->checkBoxSimulateQueue->setChecked(settings.value("PlayerManager/SimulateQueue", false).toBool());
    ui->lineEditQueuePlaceInLine->setText(settings.value("PlayerManager/QueueNumberOfPlayersInLine", "").toString());
    ui->lineEditQueueNumberOfPlayersInLine->setText(settings.value("PlayerManager/QueueNumberOfPlayersInLine", "").toString());
    ui->checkBoxShowNewsOnLogin->setChecked(settings.value("PlayerManager/ShowNewsOnLogin", false).toBool());
    ui->lineEditNewsUrl->setText(settings.value("PlayerManager/NewsUrl", "").toString());

    // Load SQLiteDBManager
    ui->lineEditSQLiteFileName->setText(settings.value("SQLiteDBManager/FileName", "").toString());
    ui->lineEditSQLiteMaxBackupNumber->setText(settings.value("SQLiteDBManager/MaxBackupNumber", "").toString());
    ui->lineEditSQLiteBackupIntervalMinutes->setText(settings.value("SQLiteDBManager/BackupIntervalMinutes", "").toString());

    // Load JsonDBManager
    ui->lineEditJsonFileName->setText(settings.value("JsonDBManager/FileName", "").toString());
    ui->lineEditJsonMaxBackupNumber->setText(settings.value("JsonDBManager/MaxBackupNumber", "").toString());
    ui->lineEditJsonBackupIntervalMinutes->setText(settings.value("JsonDBManager/BackupIntervalMinutes", "").toString());
    ui->lineEditJsonPlayerName->setText(settings.value("JsonDBManager/PlayerName", "").toString());

    // Load GroupingManager settings
    ui->lineEditMotdPlayerName->setText(settings.value("GroupingManager/MotdPlayerName", "").toString());
    ui->textEditMotdText->setPlainText(settings.value("GroupingManager/MotdText", "").toString());
    ui->comboBoxMotdPrestigeLevel->setCurrentIndex(settings.value("GroupingManager/MotdPrestigeLevel", 0).toInt());

    // Load GameData settings
    ui->checkBoxLoadAllPrototypes->setChecked(settings.value("GameData/LoadAllPrototypes", false).toBool());
    ui->checkBoxUseEquipmentSlotTableCache->setChecked(settings.value("GameData/UseEquipmentSlotTableCache", false).toBool());

    // Load GameOptions settings
    ui->checkBoxTeamUpSystemEnabled->setChecked(settings.value("GameOptions/TeamUpSystemEnabled", false).toBool());
    ui->checkBoxAchievementsEnabled->setChecked(settings.value("GameOptions/AchievementsEnabled", false).toBool());
    ui->checkBoxOmegaMissionsEnabled->setChecked(settings.value("GameOptions/OmegaMissionsEnabled", false).toBool());
    ui->checkBoxVeteranRewardsEnabled->setChecked(settings.value("GameOptions/VeteranRewardsEnabled", false).toBool());
    ui->checkBoxMultiSpecRewardsEnabled->setChecked(settings.value("GameOptions/TeamUpSystemEnabled", false).toBool());
    ui->checkBoxGiftingEnabled->setChecked(settings.value("GameOptions/GiftingEnabled", false).toBool());
    ui->checkBoxCharacterSelectV2Enabled->setChecked(settings.value("GameOptions/CharacterSelectV2Enabled", false).toBool());
    ui->checkBoxCommunityNewsV2Enabled->setChecked(settings.value("GameOptions/CommunityNewsV2Enabled", false).toBool());
    ui->checkBoxLeaderboardsEnabled->setChecked(settings.value("GameOptions/LeaderboardsEnabled", false).toBool());
    ui->checkBoxNewPlayerExperienceEnabled->setChecked(settings.value("GameOptions/NewPlayerExperienceEnabled", false).toBool());
    ui->checkBoxMissionTrackerV2Enabled->setChecked(settings.value("GameOptions/MissionTrackerV2Enabled", false).toBool());
    ui->lineEditGiftingAccountAgeInDaysRequired->setText(settings.value("GameOptions/GiftingAccountAgeInDaysRequired", "").toString());
    ui->lineEditGiftingAvatarLevelRequired->setText(settings.value("GameOptions/GiftingAvatarLevelRequired", "").toString());
    ui->lineEditGiftingLoginCountRequired->setText(settings.value("GameOptions/GiftingLoginCountRequired", "").toString());
    ui->checkBoxInfinitySystemEnabled->setChecked(settings.value("GameOptions/InfinitySystemEnabled", false).toBool());
    ui->lineEditChatBanVoteAccountAgeInDaysRequired->setText(settings.value("GameOptions/ChatBanVoteAccountAgeInDaysRequired", "").toString());
    ui->lineEditChatBanVoteAvatarLevelRequired->setText(settings.value("GameOptions/ChatBanVoteAvatarLevelRequired", "").toString());
    ui->lineEditChatBanVoteLoginCountRequired->setText(settings.value("GameOptions/ChatBanVoteLoginCountRequired", "").toString());
    ui->checkBoxIsDifficultySliderEnabled->setChecked(settings.value("GameOptions/IsDifficultySliderEnabled", false).toBool());
    ui->checkBoxOrbisTrophiesEnabled->setChecked(settings.value("GameOptions/OrbisTrophiesEnabled", false).toBool());

    // Load CustomGameOptions settings
    ui->lineEditRegionCleanupIntervalMS->setText(settings.value("CustomGameOptions/RegionCleanupIntervalMS", "").toString());
    ui->lineEditRegionUnvisitedThresholdMS->setText(settings.value("CustomGameOptions/RegionUnvisitedThresholdMS", "").toString());
    ui->checkBoxDisableMovementPowerChargeCost->setChecked(settings.value("CustomGameOptions/DisableMovementPowerChargeCost", false).toBool());
    ui->checkBoxDisableInstancedLoot->setChecked(settings.value("CustomGameOptions/DisableInstancedLoot", false).toBool());
    ui->lineEditLootSpawnGridCellRadius->setText(settings.value("CustomGameOptions/LootSpawnGridCellRadius", "").toString());

    // Load Billing settings
    ui->lineEditCurrencyBalance->setText(settings.value("Billing/CurrencyBalance", "").toString());
    ui->checkBoxApplyCatalogPatch->setChecked(settings.value("Billing/ApplyCatalogPatch", false).toBool());
    ui->checkBoxOverrideStoreUrls->setChecked(settings.value("Billing/OverrideStoreUrls", false).toBool());
    ui->lineEditStoreHomePageUrl->setText(settings.value("Billing/StoreHomePageUrl", "").toString());
    ui->lineEditStoreHomeBannerPageUrl->setText(settings.value("Billing/StoreHomeBannerPageUrl", "").toString());
    ui->lineEditStoreHeroesBannerPageUrl->setText(settings.value("Billing/StoreHeroesBannerPageUrl", "").toString());
    ui->lineEditStoreCostumesBannerPageUrl->setText(settings.value("Billing/StoreCostumesBannerPageUrl", "").toString());
    ui->lineEditStoreBoostsBannerPageUrl->setText(settings.value("Billing/StoreBoostsBannerPageUrl", "").toString());
    ui->lineEditStoreChestsBannerPageUrl->setText(settings.value("Billing/StoreChestsBannerPageUrl", "").toString());
    ui->lineEditStoreSpecialsBannerPageUrl->setText(settings.value("Billing/StoreSpecialsBannerPageUrl", "").toString());
    ui->lineEditStoreRealMoneyUrl->setText(settings.value("Billing/StoreRealMoneyUrl", "").toString());

    // Enable all group boxes
    ui->groupBoxLogging->setEnabled(true);
    ui->groupBoxFrontend->setEnabled(true);
    ui->groupBoxAuth->setEnabled(true);
    ui->groupBoxPlayerManager->setEnabled(true);
    ui->groupBoxSQLiteDBManager->setEnabled(true);
    ui->groupBoxJsonDBManager->setEnabled(true);
    ui->groupBoxGroupingManager->setEnabled(true);
    ui->groupBoxGameData->setEnabled(true);
    ui->groupBoxGameOptions->setEnabled(true);
    ui->groupBoxCustomGameOptions->setEnabled(true);
    ui->groupBoxBilling->setEnabled(true);

    QMessageBox::information(this, "Success", "Config loaded successfully!");
}

void MainWindow::onPushButtonSaveConfigClicked() {
    QString configFilePath = ui->mhServerPathEdit->text() + "/MHServerEmu/config.ini";

    // Read the existing config.ini file
    QFile file(configFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", QString("Failed to open config.ini at %1").arg(configFilePath));
        return;
    }

    QTextStream in(&file);
    QStringList lines = in.readAll().split('\n');
    file.close();

    // Map settings dynamically from UI
    QMap<QString, QString> updatedSettings = {
        // Logging settings
        {"Logging/EnableLogging", ui->checkBoxEnableLogging->isChecked() ? "true" : "false"},
        {"Logging/SynchronousMode", ui->checkBoxSynchronousMode->isChecked() ? "true" : "false"},
        {"Logging/HideSensitiveInformation", ui->checkBoxHideSensitiveInformation->isChecked() ? "true" : "false"},
        {"Logging/EnableConsole", ui->checkBoxEnableConsole->isChecked() ? "true" : "false"},
        {"Logging/ConsoleIncludeTimestamps", ui->checkBoxConsoleIncludeTimestamps->isChecked() ? "true" : "false"},
        {"Logging/ConsoleMinLevel", QString::number(ui->comboBoxConsoleMinLevel->currentIndex())},
        {"Logging/ConsoleMaxLevel", QString::number(ui->comboBoxConsoleMaxLevel->currentIndex())},
        {"Logging/FileIncludeTimestamps", ui->checkBoxFileIncludeTimestamps->isChecked() ? "true" : "false"},
        {"Logging/FileMinLevel", QString::number(ui->comboBoxFileMinLevel->currentIndex())},
        {"Logging/FileMaxLevel", QString::number(ui->comboBoxFileMaxLevel->currentIndex())},

        // Frontend settings
        {"Frontend/BindIP", ui->lineEditBindIP->text()},
        {"Frontend/Port", ui->lineEditPort->text()},
        {"Frontend/PublicAddress", ui->lineEditPublicAddress->text()},
        {"Frontend/ReceiveTimeoutMS", ui->lineEditReceiveTimeoutMS->text()},
        {"Frontend/SendTimeoutMS", ui->lineEditSendTimeoutMS->text()},

        // Auth settings
        {"Auth/Address", ui->lineEditAuthAddress->text()},
        {"Auth/Port", ui->lineEditAuthPort->text()},
        {"Auth/EnableWebApi", ui->checkBoxEnableWebApi->isChecked() ? "true" : "false"},

        // PlayerManager settings
        {"PlayerManager/UseJsonDBManager", ui->checkBoxUseJsonDBManager->isChecked() ? "true" : "false"},
        {"PlayerManager/IgnoreSessionToken", ui->checkBoxIgnoreSessionToken->isChecked() ? "true" : "false"},
        {"PlayerManager/AllowClientVersionMismatch", ui->checkBoxAllowClientVersionMismatch->isChecked() ? "true" : "false"},
        {"PlayerManager/SimulateQueue", ui->checkBoxSimulateQueue->isChecked() ? "true" : "false"},
        {"PlayerManager/QueuePlaceInLine", ui->lineEditQueuePlaceInLine->text()},
        {"PlayerManager/QueueNumberOfPlayersInLine", ui->lineEditQueueNumberOfPlayersInLine->text()},
        {"PlayerManager/ShowNewsOnLogin", ui->checkBoxShowNewsOnLogin->isChecked() ? "true" : "false"},
        {"PlayerManager/NewsUrl", ui->lineEditNewsUrl->text()},

        // SQLiteDBManager settings
        {"SQLiteDBManager/FileName", ui->lineEditSQLiteFileName->text()},
        {"SQLiteDBManager/MaxBackupNumber", ui->lineEditSQLiteMaxBackupNumber->text()},
        {"SQLiteDBManager/BackupIntervalMinutes", ui->lineEditSQLiteBackupIntervalMinutes->text()},

        // JsonDBManager settings
        {"JsonDBManager/FileName", ui->lineEditJsonFileName->text()},
        {"JsonDBManager/MaxBackupNumber", ui->lineEditJsonMaxBackupNumber->text()},
        {"JsonDBManager/BackupIntervalMinutes", ui->lineEditJsonBackupIntervalMinutes->text()},
        {"JsonDBManager/PlayerName", ui->lineEditJsonPlayerName->text()},

        // GroupingManager settings
        {"GroupingManager/MotdPlayerName", ui->lineEditMotdPlayerName->text()},
        {"GroupingManager/MotdText", ui->textEditMotdText->toPlainText()},
        {"GroupingManager/MotdPrestigeLevel", QString::number(ui->comboBoxMotdPrestigeLevel->currentIndex())},

        // GameData settings
        {"GameData/LoadAllPrototypes", ui->checkBoxLoadAllPrototypes->isChecked() ? "true" : "false"},
        {"GameData/UseEquipmentSlotTableCache", ui->checkBoxUseEquipmentSlotTableCache->isChecked() ? "true" : "false"},

        // GameOptions settings
        {"GameOptions/TeamUpSystemEnabled", ui->checkBoxTeamUpSystemEnabled->isChecked() ? "true" : "false"},
        {"GameOptions/AchievementsEnabled", ui->checkBoxAchievementsEnabled->isChecked() ? "true" : "false"},
        {"GameOptions/OmegaMissionsEnabled", ui->checkBoxOmegaMissionsEnabled->isChecked() ? "true" : "false"},
        {"GameOptions/VeteranRewardsEnabled", ui->checkBoxVeteranRewardsEnabled->isChecked() ? "true" : "false"},
        {"GameOptions/MultiSpecRewardsEnabled", ui->checkBoxMultiSpecRewardsEnabled->isChecked() ? "true" : "false"},
        {"GameOptions/GiftingEnabled", ui->checkBoxGiftingEnabled->isChecked() ? "true" : "false"},
        {"GameOptions/CharacterSelectV2Enabled", ui->checkBoxCharacterSelectV2Enabled->isChecked() ? "true" : "false"},
        {"GameOptions/CommunityNewsV2Enabled", ui->checkBoxCommunityNewsV2Enabled->isChecked() ? "true" : "false"},
        {"GameOptions/LeaderboardsEnabled", ui->checkBoxLeaderboardsEnabled->isChecked() ? "true" : "false"},
        {"GameOptions/NewPlayerExperienceEnabled", ui->checkBoxNewPlayerExperienceEnabled->isChecked() ? "true" : "false"},
        {"GameOptions/MissionTrackerV2Enabled", ui->checkBoxMissionTrackerV2Enabled->isChecked() ? "true" : "false"},
        {"GameOptions/GiftingAccountAgeInDaysRequired", ui->lineEditGiftingAccountAgeInDaysRequired->text()},
        {"GameOptions/GiftingAvatarLevelRequired", ui->lineEditGiftingAvatarLevelRequired->text()},
        {"GameOptions/GiftingLoginCountRequired", ui->lineEditGiftingLoginCountRequired->text()},
        {"GameOptions/InfinitySystemEnabled", ui->checkBoxInfinitySystemEnabled->isChecked() ? "true" : "false"},
        {"GameOptions/ChatBanVoteAccountAgeInDaysRequired", ui->lineEditChatBanVoteAccountAgeInDaysRequired->text()},
        {"GameOptions/ChatBanVoteAvatarLevelRequired", ui->lineEditChatBanVoteAvatarLevelRequired->text()},
        {"GameOptions/IsDifficultySliderEnabled", ui->checkBoxIsDifficultySliderEnabled->isChecked() ? "true" : "false"},
        {"GameOptions/OrbisTrophiesEnabled", ui->checkBoxOrbisTrophiesEnabled->isChecked() ? "true" : "false"},

        // CustomGameOptions settings
        {"CustomGameOptions/RegionCleanupIntervalMS", ui->lineEditRegionCleanupIntervalMS->text()},
        {"CustomGameOptions/RegionUnvisitedThresholdMS", ui->lineEditRegionUnvisitedThresholdMS->text()},
        {"CustomGameOptions/DisableMovementPowerChargeCost", ui->checkBoxDisableMovementPowerChargeCost->isChecked() ? "true" : "false"},
        {"CustomGameOptions/DisableInstancedLoot", ui->checkBoxDisableInstancedLoot->isChecked() ? "true" : "false"},
        {"CustomGameOptions/LootSpawnGridCellRadius", ui->lineEditLootSpawnGridCellRadius->text()},

        // Billing settings
        {"Billing/CurrencyBalance", ui->lineEditCurrencyBalance->text()},
        {"Billing/ApplyCatalogPatch", ui->checkBoxApplyCatalogPatch->isChecked() ? "true" : "false"},
        {"Billing/OverrideStoreUrls", ui->checkBoxOverrideStoreUrls->isChecked() ? "true" : "false"},
        {"Billing/StoreHomePageUrl", ui->lineEditStoreHomePageUrl->text()},
        {"Billing/StoreHomeBannerPageUrl", ui->lineEditStoreHomeBannerPageUrl->text()},
        {"Billing/StoreHeroesBannerPageUrl", ui->lineEditStoreHeroesBannerPageUrl->text()},
        {"Billing/StoreCostumesBannerPageUrl", ui->lineEditStoreCostumesBannerPageUrl->text()},
        {"Billing/StoreBoostsBannerPageUrl", ui->lineEditStoreBoostsBannerPageUrl->text()},
        {"Billing/StoreChestsBannerPageUrl", ui->lineEditStoreChestsBannerPageUrl->text()},
        {"Billing/StoreSpecialsBannerPageUrl", ui->lineEditStoreSpecialsBannerPageUrl->text()},
        {"Billing/StoreRealMoneyUrl", ui->lineEditStoreRealMoneyUrl->text()}
    };

    // Modify the settings in the existing lines while keeping comments
    QString currentSection; // Track the current section
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();

        // Skip empty lines and comments
        if (line.isEmpty() || line.startsWith(';')) {
            continue;
        }

        // Check if the line defines a section
        if (line.startsWith('[') && line.endsWith(']')) {
            currentSection = line.mid(1, line.length() - 2); // Extract section name
            continue;
        }

        // Process lines containing '=' within a section
        if (line.contains('=') && !currentSection.isEmpty()) {
            QString key = currentSection + '/' + line.section('=', 0, 0).trimmed(); // Combine section and key
            if (updatedSettings.contains(key)) {
                qDebug() << "Updating key:" << key
                         << "Old Value:" << line.section('=', 1).trimmed()
                         << "New Value:" << updatedSettings[key];
                lines[i] = QString("%1=%2").arg(line.section('=', 0, 0).trimmed()).arg(updatedSettings[key]); // Update the value
                updatedSettings.remove(key); // Remove the updated key from the map
            }
        }
    }

    // Write back to the config.ini file
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", QString("Failed to save config.ini at %1").arg(configFilePath));
        return;
    }

    QTextStream out(&file);
    for (const QString &line : lines) {
        out << line << '\n';
    }
    file.close();

    QMessageBox::information(this, "Success", "Configuration saved successfully.");
}

void MainWindow::onUpdateLevelButtonClicked() {
    if (serverProcess->state() != QProcess::Running) {
        QMessageBox::warning(this, "Error", "Server is not running. Start the server first.");
        return;
    }

    QString accountName = ui->accountNameEdit->text().trimmed();
    QString accountLevel = ui->accountLevelComboBox->currentText();

    if (accountName.isEmpty() || accountLevel.isEmpty()) {
        QMessageBox::warning(this, "Error", "Account name or level cannot be empty.");
        return;
    }

    QString command = QString("!account userlevel %1 %2\n").arg(accountName, accountLevel);
    serverProcess->write(command.toUtf8());
    ui->ServerOutputEdit->append(QString("Sent command: %1").arg(command)); // Optional feedback
}

void MainWindow::onPushButtonBanClicked() {
    if (serverProcess->state() != QProcess::Running) {
        QMessageBox::warning(this, "Error", "Server is not running. Start the server first.");
        return;
    }

    QString accountName = ui->accountNameBan->text().trimmed();

    if (accountName.isEmpty()) {
        QMessageBox::warning(this, "Error", "Account name cannot be empty.");
        return;
    }

    QString command = QString("!account ban %1\n").arg(accountName);
    serverProcess->write(command.toUtf8());
    ui->ServerOutputEdit->append(QString("Sent command: %1").arg(command)); // Optional feedback
}

void MainWindow::onPushButtonUnBanClicked() {
    if (serverProcess->state() != QProcess::Running) {
        QMessageBox::warning(this, "Error", "Server is not running. Start the server first.");
        return;
    }

    QString accountName = ui->accountNameBan->text().trimmed();

    if (accountName.isEmpty()) {
        QMessageBox::warning(this, "Error", "Account name cannot be empty.");
        return;
    }

    QString command = QString("!account unban %1\n").arg(accountName);
    serverProcess->write(command.toUtf8());
    ui->ServerOutputEdit->append(QString("Sent command: %1").arg(command)); // Optional feedback
}
void MainWindow::onKickButtonClicked() {
    if (serverProcess->state() != QProcess::Running) {
        QMessageBox::warning(this, "Error", "Server is not running. Start the server first.");
        return;
    }

    QString playerName = ui->accountKick->text().trimmed();

    if (playerName.isEmpty()) {
        QMessageBox::warning(this, "Error", "Player name cannot be empty.");
        return;
    }

    QString command = QString("!client kick %1\n").arg(playerName);
    serverProcess->write(command.toUtf8());
    ui->ServerOutputEdit->append(QString("Sent command: %1").arg(command)); // Optional feedback
}

void MainWindow::updateServerStatus() {
    // Check MHServerEmu.exe status
    QProcess process;
    process.start("tasklist", {"/fi", "imagename eq MHServerEmu.exe", "/nh"});
    process.waitForFinished();
    QString output = process.readAllStandardOutput().trimmed();
    bool isMHServerRunning = output.contains("MHServerEmu.exe");
    ui->mhServerStatusLabel->setPixmap(isMHServerRunning ? onPixmap : offPixmap);

    // Check httpd.exe status
    process.start("tasklist", {"/fi", "imagename eq httpd.exe", "/nh"});
    process.waitForFinished();
    output = process.readAllStandardOutput().trimmed();
    bool isApacheRunning = output.contains("httpd.exe");
    ui->apacheServerStatusLabel->setPixmap(isApacheRunning ? onPixmap : offPixmap);
}

void MainWindow::onServerPathEditUpdated() {
    QSettings settings("PTM", "MHServerEmuUI");
    settings.setValue("serverPath", ui->mhServerPathEdit->text());
}

void MainWindow::onPushButtonSendToServerClicked() {
    // Ensure the server is running
    if (serverProcess->state() != QProcess::Running) {
        QMessageBox::warning(this, "Error", "Server is not running.");
        return;
    }

    // Get the text from lineEditSendToServer
    QString command = ui->lineEditSendToServer->text().trimmed();

    // Ensure the command is not empty
    if (command.isEmpty()) {
        QMessageBox::warning(this, "Error", "Command cannot be empty.");
        return;
    }

    // Send the command to the server
    serverProcess->write(command.toUtf8() + "\n");
    ui->ServerOutputEdit->append("Sent command to server: " + command);

    // Clear the lineEditSendToServer after sending
    ui->lineEditSendToServer->clear();
}
