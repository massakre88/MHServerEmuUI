#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDir>
#include <QSettings>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
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
#include <QInputDialog>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFontDatabase>
#include <QRandomGenerator>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , apacheProcess(new QProcess(this))
    , serverProcess(new QProcess(this))
    , playerCount(0)
    , statusCheckTimer(new QTimer(this)) // Initialize the timer in the initializer list

{
    ui->setupUi(this);

    int fontId = QFontDatabase::addApplicationFont(":/Segoe IU.ttf");
    if (fontId != -1) {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
        if (!fontFamilies.isEmpty()) {
            QFont appFont(fontFamilies.first(), 9);  // Set default font size
            QApplication::setFont(appFont);
        }
    } else {
        qDebug() << "Failed to load custom font!";
    }

    // Load status indicator images
    onPixmap = QPixmap(":/images/on");
    offPixmap = QPixmap(":/images/off");
    if (onPixmap.isNull()) {
        qDebug() << "Failed to load /images/on from resource file.";
    }
    if (offPixmap.isNull()) {
        qDebug() << "Failed to load /images/off from resource file.";
    }

    // Set up user list context menu
    setupUserListContextMenu();

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

    // Initialize event states based on LiveTuningData.json
    initializeEventStates();

    connect(ui->pushButtonShutdown, &QPushButton::clicked, this, &MainWindow::onPushButtonShutdownClicked);
    connect(ui->loadLiveTuningButton, &QPushButton::clicked, this, &MainWindow::onLoadLiveTuning);
    connect(ui->saveLiveTuningButton, &QPushButton::clicked, this, &MainWindow::onSaveLiveTuning);
    connect(ui->reloadLiveTuningButton, &QPushButton::clicked, this, &MainWindow::onReloadLiveTuning);
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseButtonClicked);
    connect(ui->mhServerPathEdit, &QLineEdit::editingFinished, this, &MainWindow::onServerPathEditUpdated);
    connect(ui->startClientButton, &QPushButton::clicked, this, &MainWindow::onStartClientButtonClicked);
    connect(ui->updateButton, &QPushButton::clicked, this, &MainWindow::onUpdateButtonClicked);
    connect(ui->startServerButton, &QPushButton::clicked, this, &MainWindow::startServer);
    connect(ui->stopServerButton, &QPushButton::clicked, this, &MainWindow::stopServer);
    connect(serverProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::readServerOutput);
    connect(serverProcess, &QProcess::readyReadStandardError, this, &MainWindow::readServerOutput);
    connect(serverProcess, &QProcess::errorOccurred, this, &MainWindow::handleServerError);
    connect(ui->createAccountButton, &QPushButton::clicked, this, &MainWindow::openAccountCreationPage);
    connect(ui->comboBoxCategory, &QComboBox::currentTextChanged, this, &MainWindow::onCategoryChanged);
    connect(ui->pushButtonAddLTsetting, &QPushButton::clicked, this, &MainWindow::onPushButtonAddLTSettingClicked);
    connect(ui->pushButtonLoadConfig, &QPushButton::clicked, this, &MainWindow::onPushButtonLoadConfigClicked);
    connect(ui->pushButtonSaveConfig, &QPushButton::clicked, this, &MainWindow::onPushButtonSaveConfigClicked);
    connect(ui->pushButtonUnBan, &QPushButton::clicked, this, &MainWindow::onPushButtonUnBanClicked);
    connect(ui->mhServerPathEdit, &QLineEdit::editingFinished, this, &MainWindow::onServerPathEditUpdated);
    connect(ui->pushButtonSendToServer, &QPushButton::clicked, this, &MainWindow::onPushButtonSendToServerClicked);
    connect(ui->horizontalSliderCosmicChaosSwitch, &QSlider::valueChanged, this, [this](int value) {
        onEventSwitchChanged("CosmicChaos", value);
    });
    connect(ui->horizontalSliderMidtownMadnessSwitch, &QSlider::valueChanged, this, [this](int value) {
        onEventSwitchChanged("MidtownMadness", value);
    });
    connect(ui->horizontalSliderArmorIncursionSwitch, &QSlider::valueChanged, this, [this](int value) {
        onEventSwitchChanged("ArmorIncursion", value);
    });
    connect(ui->horizontalSliderOdinsBountySwitch, &QSlider::valueChanged, this, [this](int value) {
        onEventSwitchChanged("OdinsBounty", value);
    });
    connect(ui->horizontalSliderDefendersXPSwitch, &QSlider::valueChanged, this, [this](int value) {
        onEventSwitchChanged("Defenders&FriendsXP", value);
    });
    connect(ui->horizontalSliderAvengersXPSwitch, &QSlider::valueChanged, this, [this](int value) {
        onEventSwitchChanged("AvengersXP", value);
    });
    connect(ui->horizontalSliderFantastic4XPSwitch, &QSlider::valueChanged, this, [this](int value) {
        onEventSwitchChanged("FantasticFourXP", value);
    });
    connect(ui->horizontalSliderGuardiansXPSwitch, &QSlider::valueChanged, this, [this](int value) {
        onEventSwitchChanged("Guardians&CosmicXP", value);
    });
    connect(ui->horizontalSliderScoundrelsXPSwitch, &QSlider::valueChanged, this, [this](int value) {
        onEventSwitchChanged("Scoundrels&VillainsXP", value);
    });
    connect(ui->horizontalSliderXmenXPSwitch, &QSlider::valueChanged, this, [this](int value) {
        onEventSwitchChanged("XMenXP", value);
    });
    connect(ui->horizontalSliderPandemoniumProtocolSwitch, &QSlider::valueChanged, this, [this](int value) {
        onPandemoniumProtocolToggle(value);
    });
    connect(ui->pushButtonRefreshUsers, &QPushButton::clicked, this, &MainWindow::refreshLoggedInUsers);
    for (int i = 1; i <= 6; ++i) {
        QSlider *eventSwitch = findChild<QSlider *>(QString("horizontalSliderCustom%1Switch").arg(i));
        if (eventSwitch) {
            connect(eventSwitch, &QSlider::valueChanged, this, [this, i](int value) {
                onCustomEventSwitchChanged(i, value);
            });
        }
    }
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
        ui->mhServerPathEdit->setText(dir); // Update the line edit
        onServerPathEditUpdated();
    }
}

void MainWindow::onServerPathEditUpdated() {
    QString newPath = ui->mhServerPathEdit->text().trimmed();
    if (newPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Server path cannot be empty.");
        return;
    }

    // Save the updated path
    QSettings settings("PTM", "MHServerEmuUI");
    settings.setValue("serverPath", newPath);

    qDebug() << "Server path updated to:" << newPath;

    // Call the function to check and copy missing event files
    verifyAndCopyEventFiles();
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
            qDebug() << "MHServerEmu.exe did not terminate in time. Forcing kill.";
            serverProcess->kill(); // Force kill if not finished
            serverProcess->waitForFinished(); // Ensure it's fully killed
        }
    } else {
        qDebug() << "MHServerEmu.exe is not running. Skipping terminate.";
    }

    // Stop Apache process
    if (apacheProcess->state() == QProcess::Running) {
        apacheProcess->terminate();
        if (!apacheProcess->waitForFinished(5000)) { // Wait up to 5 seconds
            qDebug() << "Apache process did not terminate in time. Forcing kill.";
            apacheProcess->kill(); // Force kill if not finished
            apacheProcess->waitForFinished(); // Ensure it's fully killed
        }
    } else {
        qDebug() << "Apache process is not running. Skipping terminate.";
    }

    // Reconnect error handling
    connect(serverProcess, &QProcess::errorOccurred, this, &MainWindow::handleServerError);

    // Fallback: Ensure both processes are killed using taskkill
    QProcess taskCheckProcess;
    taskCheckProcess.start("tasklist", QStringList() << "/FI" << "IMAGENAME eq MHServerEmu.exe");
    taskCheckProcess.waitForFinished();
    QString taskCheckOutput = QString::fromLocal8Bit(taskCheckProcess.readAllStandardOutput());
    if (taskCheckOutput.contains("MHServerEmu.exe")) {
        QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "MHServerEmu.exe");
        qDebug() << "Fallback: MHServerEmu.exe killed.";
    } else {
        qDebug() << "Fallback: MHServerEmu.exe is not running.";
    }

    taskCheckProcess.start("tasklist", QStringList() << "/FI" << "IMAGENAME eq httpd.exe");
    taskCheckProcess.waitForFinished();
    taskCheckOutput = QString::fromLocal8Bit(taskCheckProcess.readAllStandardOutput());
    if (taskCheckOutput.contains("httpd.exe")) {
        QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "httpd.exe");
        qDebug() << "Fallback: httpd.exe killed.";
    } else {
        qDebug() << "Fallback: httpd.exe is not running.";
    }

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

void MainWindow::openAccountCreationPage() {
    QUrl url("http://localhost:8080/AccountManagement/Create");
    if (!QDesktopServices::openUrl(url)) {
        QMessageBox::warning(this, "Error", "Failed to open the web browser. Please check your default browser settings.");
    }
}

void MainWindow::readServerOutput() {
    static bool isProcessingClientInfo = false;
    static QString clientInfoBuffer;

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

    QStringList lines = outputText.split('\n', Qt::SkipEmptyParts);

    // ✅ Use indexed access instead of a range-based for-loop to avoid detachment
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);  // Direct access, prevents detachment

        // Check for the start of client info
        if (line.contains("SessionId:") && !isProcessingClientInfo) {
            isProcessingClientInfo = true;
            clientInfoBuffer.clear();
        }

        // Accumulate client info lines
        if (isProcessingClientInfo) {
            clientInfoBuffer.append(line + '\n');

            // Detect the end of the client info block
            if (!line.contains(":")) { // End of client info block
                isProcessingClientInfo = false;

                // Extract the SessionId from the buffer
                static const QRegularExpression regex(R"(SessionId:\s*(\S+))");
                QRegularExpressionMatch match = regex.match(clientInfoBuffer);

                QString sessionId;
                if (match.hasMatch()) {
                    sessionId = match.captured(1).trimmed();
                }

                // Retrieve username and email from the map
                QString username, email;
                if (userInfoMap.contains(sessionId)) {
                    username = userInfoMap[sessionId].first;
                    email = userInfoMap[sessionId].second;
                    userInfoMap.remove(sessionId); // Cleanup after use
                }

                // Call displayUserInfo with complete context
                displayUserInfo(clientInfoBuffer.trimmed(), username, email);
                qDebug() << "User info block processed:\n" << clientInfoBuffer;

                clientInfoBuffer.clear();
            }
        }
    }

    // Check for player connection logs
    if (outputText.contains("[PlayerConnectionManager] Accepted and registered client")) {
        playerCount++; // Increment player count
        parseLoginEvent(outputText); // Parse the login event to extract user details
    }
    // Check for player disconnection logs
    else if (outputText.contains("[PlayerConnectionManager] Removed client")) {
        static const QRegularExpression regex(R"(\[Account=(.+) \(.*?\), SessionId=(0x[0-9A-Fa-f]+)\])");
        QRegularExpressionMatch match = regex.match(outputText);
        if (match.hasMatch()) {
            QString username = match.captured(1).trimmed();
            QString sessionId = match.captured(2).trimmed();
            removeUserFromList(sessionId); // Remove user from the list
            removeUserFromLoggedInMap(sessionId); // Remove user from the map
            qDebug() << "Logged out user removed:" << username << "SessionId:" << sessionId;
        } else {
            qDebug() << "Failed to parse logout event.";
        }
        playerCount--; // Decrement player count
        if (playerCount < 0) playerCount = 0; // Ensure count doesn't go negative
    }

    // Check for server shutdown
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
   QMessageBox::critical(this, "Error", "An error occurred in the server process.");
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

void MainWindow::parseLoginEvent(const QString &logLine) {
    static const QRegularExpression regex(R"(\[Account=(.+) \(0x[0-9A-Fa-f]+\), SessionId=(0x[0-9A-Fa-f]+)\])");
    QRegularExpressionMatch match = regex.match(logLine);

    if (match.hasMatch()) {
        QString accountName = match.captured(1);
        QString sessionId = match.captured(2);

        // Store the user details
        loggedInUsers[sessionId] = accountName;
        qDebug() << "Logged in user added:" << accountName << "SessionId:" << sessionId;

        // Update the list
        refreshLoggedInUsers();
    }
}

void MainWindow::refreshLoggedInUsers() {
    ui->listWidgetLoggedInUsers->clear(); // Clear the current list

    for (auto it = loggedInUsers.begin(); it != loggedInUsers.end(); ++it) {
        QListWidgetItem *item = new QListWidgetItem(it.value(), ui->listWidgetLoggedInUsers); // Use username as text
        item->setData(Qt::UserRole, it.key()); // Set session ID as UserRole data
        ui->listWidgetLoggedInUsers->addItem(item);
        qDebug() << "Refreshed user:" << it.value() << "SessionId:" << it.key();
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

    if (!runningProcesses.isEmpty()) {
        QString message = "The following server processes are running:\n";
        message += runningProcesses.join("\n");
        message += "\n\nPlease stop the server before updating.";
        QMessageBox::warning(this, "Server Running", message);
        return;
    }

    // Define the nightly build time in GMT+0
    QTime nightlyBuildTime(7, 15, 0); // 7:15 AM GMT+0
    QDateTime nowUTC = QDateTime::currentDateTimeUtc();
    QDate nightlyDate = nowUTC.date();
    if (nowUTC.time() < nightlyBuildTime) {
        nightlyDate = nightlyDate.addDays(-1); // Use the previous day's date if before build time
    }

    QString currentDate = nightlyDate.toString("yyyyMMdd");
    QString downloadUrl = QString("https://nightly.link/Crypto137/MHServerEmu/workflows/nightly-release-windows-x64/master/MHServerEmu-nightly-%1-Release-windows-x64.zip").arg(currentDate);
    qDebug() << "Generated download URL: " << downloadUrl;
    QString serverPath = ui->mhServerPathEdit->text();
    if (serverPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Server path is empty. Set the path first.");
        return;
    }

    QString zipFilePath = serverPath + "/MHServerEmu-nightly.zip";
    QString extractPath = serverPath + "/MHServerEmu";

    // Back up files to preserve
    QStringList filesToBackup;
    if (ui->checkBoxUpdateConfigFile->isChecked()) {
        filesToBackup.append("config.ini");
    }
    if (ui->checkBoxUpdateSaveLiveTuning->isChecked()) {
        filesToBackup.append("Data/Game/LiveTuningData.json");
    }

    // Add files from the Billing folder if the checkbox is checked
    if (ui->checkBoxUpdateSaveStore->isChecked()) {
        filesToBackup.append("Data/Billing/Catalog.json"); // Replace with actual file names
        filesToBackup.append("Data/Billing/CatalogPatch.json");
    }

    QMap<QString, QString> backupFiles;
    for (const QString &file : filesToBackup) {
        QString originalPath = extractPath + "/" + file;
        QString backupPath = originalPath + ".bak";
        if (QFile::exists(originalPath)) {
            if (QFile::rename(originalPath, backupPath)) {
                backupFiles.insert(originalPath, backupPath);
                qDebug() << "Backed up file:" << originalPath << "to" << backupPath;
            } else {
                QMessageBox::critical(this, "Error", "Failed to back up file: " + originalPath);
                return;
            }
        }
    }

    // Use curl to download the nightly build
    QProcess *curlProcess = new QProcess(this);
    QStringList curlArguments = {"-L", "--silent", "-o", zipFilePath, downloadUrl};

    connect(curlProcess, &QProcess::readyReadStandardOutput, this, [=]() {
        qDebug() << "Curl Output:" << curlProcess->readAllStandardOutput();
    });

    connect(curlProcess, &QProcess::readyReadStandardError, this, [=]() {
        qDebug() << "Curl Error:" << curlProcess->readAllStandardError();
    });

    connect(curlProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            if (!QFile::exists(zipFilePath)) {
                QMessageBox::critical(this, "Error", "The ZIP file does not exist after downloading.");
                return;
            }

            // Extract the ZIP file
            QStringList args = {
                "-NoProfile",
                "-ExecutionPolicy", "Bypass",
                "-Command",
                QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force").arg(zipFilePath, extractPath)
            };

            QProcess *extractProcess = new QProcess(this);
            extractProcess->setProgram("powershell");
            extractProcess->setArguments(args);

            connect(extractProcess, &QProcess::readyReadStandardOutput, this, [=]() {
                qDebug() << "PowerShell Output:" << extractProcess->readAllStandardOutput();
            });

            connect(extractProcess, &QProcess::readyReadStandardError, this, [=]() {
                qDebug() << "PowerShell Error:" << extractProcess->readAllStandardError();
            });

            connect(extractProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                    // Restore backed-up files
                    for (auto it = backupFiles.begin(); it != backupFiles.end(); ++it) {
                        QString originalPath = it.key();
                        QString backupPath = it.value();
                        if (QFile::exists(originalPath)) {
                            QFile::remove(originalPath);
                        }
                        if (QFile::rename(backupPath, originalPath)) {
                            qDebug() << "Restored file:" << backupPath << "to" << originalPath;
                        } else {
                            QMessageBox::critical(this, "Error", "Failed to restore file: " + backupPath);
                        }
                    }

                    QFile::remove(zipFilePath); // Clean up ZIP file
                    QMessageBox::information(this, "Update", "Update completed successfully.");
                } else {
                    QMessageBox::critical(this, "Error", "Failed to extract update files.");
                }
            });

            extractProcess->start();
        } else {
            QMessageBox::critical(this, "Error", "Failed to download update via curl.");
        }
    });

    curlProcess->start("curl", curlArguments);
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

void MainWindow::verifyAndCopyEventFiles() {
    // Define the server's LiveTuning folder
    QString serverPath = ui->mhServerPathEdit->text() + "/MHServerEmu/Data/Game/";

    // Define the source folder (where the program's EXE is located)
    QString sourcePath = QCoreApplication::applicationDirPath() + "/";

    // List of event file names
    QStringList eventFiles = {
        "LiveTuningData_CosmicChaos.json",
        "OFF_LiveTuningData_CosmicChaos.json",
        "LiveTuningData_MidtownMadness.json",
        "OFF_LiveTuningData_MidtownMadness.json",
        "LiveTuningData_ArmorIncursion.json",
        "OFF_LiveTuningData_ArmorIncursion.json",
        "LiveTuningData_OdinsBounty.json",
        "OFF_LiveTuningData_OdinsBounty.json",
        "LiveTuningData_Defenders&FriendsXP.json",
        "OFF_LiveTuningData_Defenders&FriendsXP.json",
        "LiveTuningData_AvengersXP.json",
        "OFF_LiveTuningData_AvengersXP.json",
        "LiveTuningData_FantasticFourXP.json",
        "OFF_LiveTuningData_FantasticFourXP.json",
        "LiveTuningData_Guardians&CosmicXP.json",
        "OFF_LiveTuningData_Guardians&CosmicXP.json",
        "LiveTuningData_Scoundrels&VillainsXP.json",
        "OFF_LiveTuningData_Scoundrels&VillainsXP.json",
        "LiveTuningData_XMenXP.json",
        "OFF_LiveTuningData_XMenXP.json",
        "OFF_LiveTuningDataz_PandemoniumProtocol.json",
        "LiveTuningDataz_PandemoniumProtocol.json"
    };

    // Ensure the server folder exists
    QDir serverDir(serverPath);
    if (!serverDir.exists()) {
        QMessageBox::critical(this, "Error", "Server folder not found. Please check the server path.");
        return;
    }

    // Copy missing event files
    for (const QString &fileName : eventFiles) {
        QString destFilePath = serverPath + fileName;
        QString sourceFilePath = sourcePath + fileName;

        if (!QFile::exists(destFilePath)) {
            if (QFile::exists(sourceFilePath)) {
                if (QFile::copy(sourceFilePath, destFilePath)) {
                    qDebug() << "Copied missing event file:" << fileName;
                } else {
                    qDebug() << "Failed to copy:" << fileName;
                }
            } else {
                qDebug() << "Source event file missing:" << sourceFilePath;
            }
        }
    }
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
        {"eAETV_", "Avatar Entity"},
        {"eATV_", "Area"},
        {"ePOTV_", "Population Object"},
        {"eMFTV_", "Metrics Frequency"},
        {"ePETV_", "Public Events"}
    };

    // Categorize items dynamically
    categories.clear();  // Reset the categories

    // ✅ Use indexed access instead of a range-based for-loop to avoid detachment
    for (int i = 0; i < jsonArray.size(); ++i) {
        QJsonValue value = jsonArray.at(i);

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

    for (auto it = categories.constBegin(); it != categories.constEnd(); ++it) {
        ui->comboBoxCategory->addItem(it.key());
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
        connect(slider, &QSlider::valueChanged, this, [lineEdit](int sliderValue) {
            double scaledValue = sliderValue / 10.0;
            lineEdit->setText(QString::number(scaledValue, 'f', 1));
        });

        connect(lineEdit, &QLineEdit::editingFinished, this, [slider, lineEdit]() {
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

        connect(slider, &QSlider::valueChanged, this, [lineEdit](int sliderValue) {
            double scaledValue = sliderValue / 10.0;
            lineEdit->setText(QString::number(scaledValue, 'f', 1));
        });

        connect(lineEdit, &QLineEdit::editingFinished, this, [slider, lineEdit]() {
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
    ui->lineEditFileChannels->setText(settings.value("Logging/FileChannels", "").toString());
    ui->checkBoxFileSplitOutput->setChecked(settings.value("Logging/FileSplitOutput", false).toBool());

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
    ui->lineEditGameInstanceCount->setText(settings.value("PlayerManager/GameInstanceCount", "").toString());
    ui->lineEditPlayerCountDivisor->setText(settings.value("PlayerManager/PlayerCountDivisor", "").toString());

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
    ui->lineEditTrashedItemExpirationTimeMultiplier->setText(settings.value("CustomGameOptions/TrashedItemExpirationTimeMultiplier", "").toString());

    // Load Billing settings
    ui->lineEditGazillioniteBalanceForNewAccounts->setText(settings.value("Billing/GazillioniteBalanceForNewAccounts", "").toString());
    ui->lineEditESToGazillioniteConversionRatio->setText(settings.value("Billing/ESToGazillioniteConversionRatio", "").toString());
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
        {"Logging/FileChannels", ui->lineEditFileChannels->text()},
        {"Logging/FileSplitOutput", ui->checkBoxFileSplitOutput->isChecked() ? "true" : "false"},
        {"Frontend/BindIP", ui->lineEditBindIP->text()},
        {"Frontend/Port", ui->lineEditPort->text()},
        {"Frontend/PublicAddress", ui->lineEditPublicAddress->text()},
        {"Frontend/ReceiveTimeoutMS", ui->lineEditReceiveTimeoutMS->text()},
        {"Frontend/SendTimeoutMS", ui->lineEditSendTimeoutMS->text()},
        {"Auth/Address", ui->lineEditAuthAddress->text()},
        {"Auth/Port", ui->lineEditAuthPort->text()},
        {"Auth/EnableWebApi", ui->checkBoxEnableWebApi->isChecked() ? "true" : "false"},
        {"PlayerManager/UseJsonDBManager", ui->checkBoxUseJsonDBManager->isChecked() ? "true" : "false"},
        {"PlayerManager/IgnoreSessionToken", ui->checkBoxIgnoreSessionToken->isChecked() ? "true" : "false"},
        {"PlayerManager/AllowClientVersionMismatch", ui->checkBoxAllowClientVersionMismatch->isChecked() ? "true" : "false"},
        {"PlayerManager/SimulateQueue", ui->checkBoxSimulateQueue->isChecked() ? "true" : "false"},
        {"PlayerManager/QueuePlaceInLine", ui->lineEditQueuePlaceInLine->text()},
        {"PlayerManager/QueueNumberOfPlayersInLine", ui->lineEditQueueNumberOfPlayersInLine->text()},
        {"PlayerManager/ShowNewsOnLogin", ui->checkBoxShowNewsOnLogin->isChecked() ? "true" : "false"},
        {"PlayerManager/NewsUrl", ui->lineEditNewsUrl->text()},
        {"PlayerManager/GameInstanceCount", ui->lineEditGameInstanceCount->text()},
        {"PlayerManager/PlayerCountDivisor", ui->lineEditPlayerCountDivisor->text()},
        {"SQLiteDBManager/FileName", ui->lineEditSQLiteFileName->text()},
        {"SQLiteDBManager/MaxBackupNumber", ui->lineEditSQLiteMaxBackupNumber->text()},
        {"SQLiteDBManager/BackupIntervalMinutes", ui->lineEditSQLiteBackupIntervalMinutes->text()},
        {"JsonDBManager/FileName", ui->lineEditJsonFileName->text()},
        {"JsonDBManager/MaxBackupNumber", ui->lineEditJsonMaxBackupNumber->text()},
        {"JsonDBManager/BackupIntervalMinutes", ui->lineEditJsonBackupIntervalMinutes->text()},
        {"JsonDBManager/PlayerName", ui->lineEditJsonPlayerName->text()},
        {"GroupingManager/MotdPlayerName", ui->lineEditMotdPlayerName->text()},
        {"GroupingManager/MotdText", ui->textEditMotdText->toPlainText()},
        {"GroupingManager/MotdPrestigeLevel", QString::number(ui->comboBoxMotdPrestigeLevel->currentIndex())},
        {"GameData/LoadAllPrototypes", ui->checkBoxLoadAllPrototypes->isChecked() ? "true" : "false"},
        {"GameData/UseEquipmentSlotTableCache", ui->checkBoxUseEquipmentSlotTableCache->isChecked() ? "true" : "false"},
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
        {"GameOptions/ChatBanVoteLoginCountRequired", ui->lineEditChatBanVoteLoginCountRequired->text()},
        {"GameOptions/IsDifficultySliderEnabled", ui->checkBoxIsDifficultySliderEnabled->isChecked() ? "true" : "false"},
        {"GameOptions/OrbisTrophiesEnabled", ui->checkBoxOrbisTrophiesEnabled->isChecked() ? "true" : "false"},
        {"CustomGameOptions/RegionCleanupIntervalMS", ui->lineEditRegionCleanupIntervalMS->text()},
        {"CustomGameOptions/RegionUnvisitedThresholdMS", ui->lineEditRegionUnvisitedThresholdMS->text()},
        {"CustomGameOptions/DisableMovementPowerChargeCost", ui->checkBoxDisableMovementPowerChargeCost->isChecked() ? "true" : "false"},
        {"CustomGameOptions/DisableInstancedLoot", ui->checkBoxDisableInstancedLoot->isChecked() ? "true" : "false"},
        {"CustomGameOptions/LootSpawnGridCellRadius", ui->lineEditLootSpawnGridCellRadius->text()},
        {"CustomGameOptions/TrashedItemExpirationTimeMultiplier", ui->lineEditTrashedItemExpirationTimeMultiplier->text()},
        {"Billing/GazillioniteBalanceForNewAccounts", ui->lineEditGazillioniteBalanceForNewAccounts->text()},
        {"Billing/ESToGazillioniteConversionRatio", ui->lineEditESToGazillioniteConversionRatio->text()},
        {"Billing/ApplyCatalogPatch", ui->checkBoxApplyCatalogPatch->isChecked() ? "true" : "false"},
        {"Billing/OverrideStoreUrls", ui->checkBoxOverrideStoreUrls->isChecked() ? "true" : "false"},
        {"Billing/StoreHomePageUrl", ui->lineEditStoreHomePageUrl->text()},
        {"Billing/StoreHomeBannerPageUrl", ui->lineEditStoreHomeBannerPageUrl->text()},
        {"Billing/StoreHeroesBannerPageUrl", ui->lineEditStoreHeroesBannerPageUrl->text()},
        {"Billing/StoreCostumesBannerPageUrl", ui->lineEditStoreCostumesBannerPageUrl->text()},
        {"Billing/StoreBoostsBannerPageUrl", ui->lineEditStoreBoostsBannerPageUrl->text()},
        {"Billing/StoreChestsBannerPageUrl", ui->lineEditStoreChestsBannerPageUrl->text()},
        {"Billing/StoreSpecialsBannerPageUrl", ui->lineEditStoreSpecialsBannerPageUrl->text()},
        {"Billing/StoreRealMoneyUrl", ui->lineEditStoreRealMoneyUrl->text()},
    };

    // Modify the settings in the existing lines while keeping comments
    QString currentSection;

    // ✅ Use indexed access to avoid detachment
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i).trimmed();

        if (line.isEmpty() || line.startsWith(';')) {
            continue;
        }

        // Check if the line defines a section
        if (line.startsWith('[') && line.endsWith(']')) {
            currentSection = line.mid(1, line.length() - 2);
            continue;
        }

        // Process key-value pairs within a section
        if (line.contains('=') && !currentSection.isEmpty()) {
            QString key = currentSection + '/' + line.section('=', 0, 0).trimmed();
            if (updatedSettings.contains(key)) {
                qDebug() << "Updating key:" << key
                         << "Old Value:" << line.section('=', 1).trimmed()
                         << "New Value:" << updatedSettings[key];

                // ✅ Fix: Use multi-arg `QString::arg` instead of chaining
                lines[i] = QString("%1=%2").arg(line.section('=', 0, 0).trimmed(), updatedSettings[key]);

                updatedSettings.remove(key);
            }
        }
    }

    // Write back to the config.ini file
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", QString("Failed to save config.ini at %1").arg(configFilePath));
        return;
    }

    QTextStream out(&file);

    // ✅ Use indexed access to prevent detachment
    for (int i = 0; i < lines.size(); ++i) {
        out << lines.at(i) << '\n';
    }

    file.close();
    QMessageBox::information(this, "Success", "Configuration saved successfully.");
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

void MainWindow::initializeEventStates() {
    QSettings settings("PTM", "MHServerEmuUI");

    int cosmicChaosState = settings.value("CosmicChaosEvent", 0).toInt();
    ui->horizontalSliderCosmicChaosSwitch->blockSignals(true);
    ui->horizontalSliderCosmicChaosSwitch->setValue(cosmicChaosState);
    ui->horizontalSliderCosmicChaosSwitch->blockSignals(false);

    int armorIncursionState = settings.value("ArmorIncursionEvent", 0).toInt();
    ui->horizontalSliderArmorIncursionSwitch->blockSignals(true);
    ui->horizontalSliderArmorIncursionSwitch->setValue(armorIncursionState);
    ui->horizontalSliderArmorIncursionSwitch->blockSignals(false);

    int midtownMadnessState = settings.value("MidtownMadnessEvent", 0).toInt();
    ui->horizontalSliderMidtownMadnessSwitch->blockSignals(true);
    ui->horizontalSliderMidtownMadnessSwitch->setValue(midtownMadnessState);
    ui->horizontalSliderMidtownMadnessSwitch->blockSignals(false);

    int odinsBountyState = settings.value("OdinsBountyEvent", 0).toInt();
    ui->horizontalSliderOdinsBountySwitch->blockSignals(true);
    ui->horizontalSliderOdinsBountySwitch->setValue(odinsBountyState);
    ui->horizontalSliderOdinsBountySwitch->blockSignals(false);
}

void MainWindow::onEventSwitchChanged(const QString &eventName, int value) {
    // Save the state using QSettings
    QSettings settings("PTM", "MHServerEmuUI");
    settings.setValue(eventName + "Event", value);

    // Define the file paths
    QString serverPath = ui->mhServerPathEdit->text() + "/MHServerEmu/Data/Game/";
    QString activeFilePath = serverPath + "LiveTuningData_" + eventName + ".json";
    QString inactiveFilePath = serverPath + "OFF_LiveTuningData_" + eventName + ".json";

    QFile activeFile(activeFilePath);
    QFile inactiveFile(inactiveFilePath);

    if (value == 1) {
        // Enabling event
        if (!inactiveFile.exists()) {
            QMessageBox::warning(this, "Error", QString("Inactive file not found: %1").arg(inactiveFilePath));
            return;
        }
        if (!inactiveFile.rename(activeFilePath)) {
            QMessageBox::critical(this, "Error", QString("Failed to enable the event: %1").arg(inactiveFile.errorString()));
            return;
        }
    } else {
        // Disabling event
        if (!activeFile.exists()) {
            QMessageBox::warning(this, "Error", QString("Active file not found: %1").arg(activeFilePath));
            return;
        }
        if (!activeFile.rename(inactiveFilePath)) {
            QMessageBox::critical(this, "Error", QString("Failed to disable the event: %1").arg(activeFile.errorString()));
            return;
        }
    }

    // Send broadcast message
    QString broadcastMessage = (value == 1)
                                   ? QString("The %1 Event has started!").arg(eventName)
                                   : QString("The %1 Event has ended!").arg(eventName);
    if (serverProcess->state() == QProcess::Running) {
        serverProcess->write(QString("!server broadcast %1\n").arg(broadcastMessage).toUtf8());
        ui->ServerOutputEdit->append(QString("Sent broadcast message: %1").arg(broadcastMessage));
        serverProcess->write("!server reloadlivetuning\n");
        ui->ServerOutputEdit->append("Sent command: !server reloadlivetuning");
    } else {
        QMessageBox::warning(this, "Error", "Server is not running.");
    }

    // Log the status
    ui->ServerOutputEdit->append(QString("%1 event %2.").arg(eventName, (value == 1) ? "enabled" : "disabled"));
}

void MainWindow::onCustomEventSwitchChanged(int eventIndex, int value) {
    // Get the corresponding line edit based on eventIndex
    QLineEdit *eventLineEdit = findChild<QLineEdit *>(QString("LineEditCustomEvent%1").arg(eventIndex));
    if (!eventLineEdit) {
        QMessageBox::warning(this, "Error", "Failed to find the event name input field.");
        return;
    }

    QString eventFileName = eventLineEdit->text().trimmed();
    if (eventFileName.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter a valid event file name.");
        return;
    }

    // Define the server folder
    QString serverPath = ui->mhServerPathEdit->text() + "/MHServerEmu/Data/Game/";
    QString activeFilePath = serverPath + eventFileName;
    QString inactiveFilePath = serverPath + "OFF_" + eventFileName;

    QFile activeFile(activeFilePath);
    QFile inactiveFile(inactiveFilePath);

    if (value == 1) {
        // Enabling the event (remove "OFF_" prefix)
        if (!inactiveFile.exists()) {
            QMessageBox::warning(this, "Error", QString("Inactive file not found: %1").arg(inactiveFilePath));
            return;
        }
        if (!inactiveFile.rename(activeFilePath)) {
            QMessageBox::critical(this, "Error", QString("Failed to enable the event: %1").arg(inactiveFile.errorString()));
            return;
        }
    } else {
        // Disabling the event (add "OFF_" prefix)
        if (!activeFile.exists()) {
            QMessageBox::warning(this, "Error", QString("Active file not found: %1").arg(activeFilePath));
            return;
        }
        if (!activeFile.rename(inactiveFilePath)) {
            QMessageBox::critical(this, "Error", QString("Failed to disable the event: %1").arg(activeFile.errorString()));
            return;
        }
    }

    // Notify user and update log
    QString status = (value == 1) ? "enabled" : "disabled";
    ui->ServerOutputEdit->append(QString("Custom Event %1 %2.").arg(eventFileName, status));

    // Reload live tuning if server is running
    if (serverProcess->state() == QProcess::Running) {
        serverProcess->write("!server reloadlivetuning\n");
        ui->ServerOutputEdit->append("Sent command: !server reloadlivetuning");
    } else {
        QMessageBox::warning(this, "Error", "Server is not running.");
    }
}

void MainWindow::setupUserListContextMenu() {
    // Enable the context menu on the QListWidget
    ui->listWidgetLoggedInUsers->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listWidgetLoggedInUsers, &QWidget::customContextMenuRequested, this, &MainWindow::showUserContextMenu);
}

void MainWindow::showUserContextMenu(const QPoint &pos) {
    QListWidgetItem *item = ui->listWidgetLoggedInUsers->itemAt(pos);
    if (!item) return; // No item under the cursor

    // Create the context menu
    QMenu contextMenu(this);

    QAction *kickUserAction = new QAction("Kick User", &contextMenu);
    QAction *banUserAction = new QAction("Ban User", &contextMenu);
    QAction *updateLevelAction = new QAction("Update Account Level", &contextMenu);
    QAction *clientInfoAction = new QAction("Client Info", &contextMenu);

    // Connect actions to slots
    connect(kickUserAction, &QAction::triggered, this, &MainWindow::kickUser);
    connect(banUserAction, &QAction::triggered, this, &MainWindow::banUser);

    // Connect "Update Account Level" action
    connect(updateLevelAction, &QAction::triggered, this, [this, item]() {
        QString username = item->text(); // The list item's text is the username
        showUpdateLevelDialog(username);
    });

    // Connect "Client Info" action
    connect(clientInfoAction, &QAction::triggered, this, [this, item]() {
        QString username = item->text(); // Assuming username is stored as the item text
        QString sessionId = item->data(Qt::UserRole).toString(); // Assuming session ID is stored in UserRole
        QString email = getEmailFromDatabase(username);

        if (email.isEmpty()) {
            QMessageBox::warning(this, "Error", "Failed to retrieve email for the account.");
            return;
        }

        // Send the client info command with session ID, username, and email
        sendClientInfoCommand(sessionId, username, email);
    });

    // Add actions to the context menu
    contextMenu.addAction(kickUserAction);
    contextMenu.addAction(banUserAction);
    contextMenu.addAction(updateLevelAction);
    contextMenu.addAction(clientInfoAction);

    // Show the context menu
    contextMenu.exec(ui->listWidgetLoggedInUsers->mapToGlobal(pos));
}

void MainWindow::showUpdateLevelDialog(const QString &username) {
    if (serverProcess->state() != QProcess::Running) {
        QMessageBox::warning(this, "Error", "Server is not running. Start the server first.");
        return;
    }

    // Retrieve the email from the database
    QString email = getEmailFromDatabase(username);
    if (email.isEmpty()) {
        QMessageBox::warning(this, "Error", "Failed to retrieve email for the account.");
        return;
    }

    // Create a dialog for selecting the account level
    QDialog dialog(this);
    dialog.setWindowTitle("Update Account Level");

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *label = new QLabel(QString("Update level for account: %1 (%2)").arg(username, email), &dialog);
    QComboBox *levelComboBox = new QComboBox(&dialog);
    levelComboBox->addItems({"User", "Moderator", "Administrator"}); // Example levels

    QPushButton *updateButton = new QPushButton("Update", &dialog);
    QPushButton *cancelButton = new QPushButton("Cancel", &dialog);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(updateButton);
    buttonLayout->addWidget(cancelButton);

    layout->addWidget(label);
    layout->addWidget(levelComboBox);
    layout->addLayout(buttonLayout);

    connect(updateButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    // Show the dialog and process the result
    if (dialog.exec() == QDialog::Accepted) {
        QString selectedLevel = levelComboBox->currentText();

        if (selectedLevel.isEmpty()) {
            QMessageBox::warning(this, "Error", "Level cannot be empty.");
            return;
        }

        // Map level names to numeric values
        int levelValue = 0; // Default to User
        if (selectedLevel == "Moderator") {
            levelValue = 1;
        } else if (selectedLevel == "Administrator") {
            levelValue = 2;
        }

        // Send the command to the server
        QString command = QString("!account userlevel %1 %2\n").arg(email).arg(levelValue);
        serverProcess->write(command.toUtf8());
        ui->ServerOutputEdit->append(QString("Sent command: %1").arg(command));
    }
}

void MainWindow::addUserToList(const QString &username, const QString &sessionId) {
    QListWidgetItem *item = new QListWidgetItem(username, ui->listWidgetLoggedInUsers);
    item->setData(Qt::UserRole, sessionId);
    qDebug() << "Set SessionId for" << username << ":" << item->data(Qt::UserRole).toString();
    ui->listWidgetLoggedInUsers->addItem(item);
    qDebug() << "Logged in user added:" << username << "SessionId:" << sessionId;
}

void MainWindow::removeUserFromList(const QString &sessionId) {
    for (int i = 0; i < ui->listWidgetLoggedInUsers->count(); ++i) {
        QListWidgetItem *item = ui->listWidgetLoggedInUsers->item(i);
        if (item->data(Qt::UserRole).toString() == sessionId) {
            qDebug() << "Removing user with SessionId:" << sessionId;
            delete ui->listWidgetLoggedInUsers->takeItem(i);
            return;
        }
    }
    qDebug() << "No user found with SessionId:" << sessionId;
    removeUserFromLoggedInMap(sessionId);
}

void MainWindow::removeUserFromLoggedInMap(const QString &sessionId) {
    if (loggedInUsers.remove(sessionId)) {
        qDebug() << "Removed user from map with SessionId:" << sessionId;
    } else {
        qDebug() << "SessionId not found in map:" << sessionId;
    }
}

void MainWindow::sendClientInfoCommand(const QString &sessionId, const QString &username, const QString &email) {
    if (sessionId.isEmpty()) {
        qDebug() << "Invalid session ID.";
        return;
    }

    // Store username and email for later use
    userInfoMap[sessionId] = {username, email}; // userInfoMap is a QMap<QString, QPair<QString, QString>>

    QString command = QString("!client info %1\n").arg(sessionId);
    serverProcess->write(command.toUtf8());
    qDebug() << "Sent command:" << command << "for Username:" << username << ", Email:" << email;
}

void MainWindow::displayUserInfo(const QString &info, const QString &username, const QString &email) {
    qDebug() << "Displaying info for Username:" << username
             << ", Email:" << email
             << "\nInfo Block:\n" << info;

    // ✅ Use multi-arg QString::arg() for better efficiency
    QString displayText = QString("Username: %1\nEmail: %2\n\n%3")
                              .arg(username, email.isEmpty() ? "N/A" : email, info);

    ui->userInfoDisplay->setPlainText(displayText);
}

void MainWindow::kickUser() {
    // Get the selected user from the list
    QListWidgetItem *item = ui->listWidgetLoggedInUsers->currentItem();
    if (!item) return;

    // Extract the player name (assuming the player's name is stored as the item text)
    QString playerName = item->text();

    // Construct and send the kick command
    QString command = QString("!client kick %1\n").arg(playerName);

    serverProcess->write(command.toUtf8());
    ui->ServerOutputEdit->append(QString("Kicked user: %1").arg(playerName));
}

void MainWindow::banUser() {
    // Get the selected user from the list
    QListWidgetItem *item = ui->listWidgetLoggedInUsers->currentItem();
    if (!item) return;

    QString username = item->text(); // Username is displayed in the list

    // Retrieve the email from the database
    QString email = getEmailFromDatabase(username);
    if (email.isEmpty()) {
        QMessageBox::warning(this, "Error", "Failed to retrieve email for the selected account.");
        return;
    }

    // Send the ban command using the email
    QString banCommand = QString("!account ban %1\n").arg(email);
    serverProcess->write(banCommand.toUtf8());
    ui->ServerOutputEdit->append(QString("Banned user: %1 (%2)").arg(username, email));

    // Automatically kick the banned user using their username
    kickUser();
}

QString MainWindow::getEmailFromDatabase(const QString &username) {
    qDebug() << "Fetching email for PlayerName:" << username;

    if (username.isEmpty()) {
        qDebug() << "PlayerName is empty, skipping database query.";
        return QString();
    }

    // Ensure the database connection
    if (!QSqlDatabase::contains("AppConnection")) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "AppConnection");
        QString serverPath = ui->mhServerPathEdit->text().trimmed();
        QString dbPath = serverPath + "/MHServerEmu/Data/account.db";
        db.setDatabaseName(dbPath);

        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            return QString();
        }
    }

    QSqlDatabase db = QSqlDatabase::database("AppConnection");

    if (!db.isOpen()) {
        qDebug() << "Database is not open.";
        return QString();
    }

    // Prepare and execute the query
    QSqlQuery query(db);
    query.prepare("SELECT Email FROM Account WHERE PlayerName = :username");
    query.bindValue(":username", username);
    qDebug() << "Executing query:" << query.executedQuery();

    if (!query.exec()) {
        qDebug() << "Query execution failed:" << query.lastError().text();
        return QString();
    }

    if (query.next()) {
        QString email = query.value(0).toString().trimmed();
        qDebug() << "Email fetched successfully:" << email;
        return email;
    }

    qDebug() << "No email found for PlayerName:" << username;
    return QString();
}

void MainWindow::onPandemoniumProtocolToggle(int value) {
    QString serverPath = ui->mhServerPathEdit->text() + "/MHServerEmu/Data/Game/";
    QString activeFilePath = serverPath + "LiveTuningDataz_PandemoniumProtocol.json";
    QString inactiveFilePath = serverPath + "OFF_LiveTuningDataz_PandemoniumProtocol.json";

    QFile activeFile(activeFilePath);
    QFile inactiveFile(inactiveFilePath);

    if (value == 1) {
        // Enable event: Rename OFF_ file to active file
        if (!inactiveFile.exists()) {
            QMessageBox::warning(this, "Error", "Pandemonium Protocol file not found!");
            return;
        }
        if (!inactiveFile.rename(activeFilePath)) {
            QMessageBox::critical(this, "Error", "Failed to activate Pandemonium Protocol.");
            return;
        }

        qDebug() << "Pandemonium Protocol enabled.";
        runPandemoniumProtocol(); // Start the event cycle

    } else {
        // Disable event: Rename active file to OFF_ file
        if (!activeFile.exists()) {
            QMessageBox::warning(this, "Error", "Pandemonium Protocol file not found!");
            return;
        }
        if (!activeFile.rename(inactiveFilePath)) {
            QMessageBox::critical(this, "Error", "Failed to deactivate Pandemonium Protocol.");
            return;
        }

        qDebug() << "Pandemonium Protocol disabled.";
    }
}

void MainWindow::runPandemoniumProtocol() {
    QString filePath = ui->mhServerPathEdit->text() + "/MHServerEmu/Data/Game/LiveTuningDataz_PandemoniumProtocol.json";
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Failed to open Pandemonium Protocol file.");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) {
        QMessageBox::critical(this, "Error", "Invalid JSON format in Pandemonium Protocol file.");
        return;
    }

    QJsonArray dataArray = doc.array();
    QMap<QString, QString> settingNames = {
        {"eGTV_VendorXPGain", "Vendor XP"},
        {"eGTV_XPGain", "XP Boost"},
        {"eGTV_LootSpecialDropRate", "SiF"},
        {"eGTV_LootRarity", "RiF"}
    };

    // Get min/max values from line edits, ensuring min is less than max
    double minBoost = ui->LineEditPandemoniumBoostRangeMin->text().toDouble();
    double maxBoost = ui->LineEditPandemoniumBoostRangeMax->text().toDouble();

    if (minBoost > maxBoost) {
        std::swap(minBoost, maxBoost); // Ensure valid range
    }

    QStringList broadcastParts;
    bool detailedBroadcast = ui->checkBoxPandemoniumBroadcast->isChecked();

    if (detailedBroadcast) {
        broadcastParts << "The Pandemonium shifts!";
    } else {
        broadcastParts << "The chaos shifts once more...";
    }

    for (int i = 0; i < dataArray.size(); ++i) {
        QJsonObject obj = dataArray[i].toObject();
        QString settingKey = obj["Setting"].toString();

        if (settingNames.contains(settingKey)) {
            double randomValue = minBoost + QRandomGenerator::global()->bounded(maxBoost - minBoost);
            randomValue = QString::number(randomValue, 'f', 2).toDouble(); // Keep 2 decimal places

            obj["Value"] = randomValue;
            dataArray[i] = obj;

            if (detailedBroadcast) {
                broadcastParts << QString("%1: %2x").arg(settingNames[settingKey]).arg(randomValue, 0, 'f', 2);
            }
        }
    }

    // Save updated JSON
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Failed to save updated Pandemonium Protocol file.");
        return;
    }
    doc.setArray(dataArray);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    // Pick a random sub-event (1-4 for an event, 5 for none)
    static QStringList subEvents = {
        "ArmorIncursion",
        "CosmicChaos",
        "MidtownMadness",
        "OdinsBounty"
    };

    QString serverPath = ui->mhServerPathEdit->text() + "/MHServerEmu/Data/Game/";

    if (!currentSubEvent.isEmpty()) {
        QString activePath = serverPath + "LiveTuningData_" + currentSubEvent + ".json";
        QString inactivePath = serverPath + "OFF_LiveTuningData_" + currentSubEvent + ".json";

        QFile activeFile(activePath);
        if (activeFile.exists()) {
            activeFile.rename(inactivePath);
        }
        currentSubEvent.clear();
    }

    int roll = QRandomGenerator::global()->bounded(1, 6);
    if (roll <= 4) {
        currentSubEvent = subEvents[roll - 1];
        QString activePath = serverPath + "LiveTuningData_" + currentSubEvent + ".json";
        QString inactivePath = serverPath + "OFF_LiveTuningData_" + currentSubEvent + ".json";

        QFile inactiveFile(inactivePath);
        if (inactiveFile.exists()) {
            inactiveFile.rename(activePath);
            if (detailedBroadcast) {
                broadcastParts << QString("Bonus Event: %1!").arg(currentSubEvent);
            }
        }
    } else if (detailedBroadcast) {
        broadcastParts << "No additional event this time...";
    }

    // Reload live tuning and broadcast
    if (serverProcess->state() == QProcess::Running) {
        serverProcess->write("!server reloadlivetuning\n");
        ui->ServerOutputEdit->append("Sent command: !server reloadlivetuning");

        QString broadcastMessage = broadcastParts.join(" ");
        serverProcess->write(QString("!server broadcast %1\n").arg(broadcastMessage).toUtf8());
        ui->ServerOutputEdit->append("Sent broadcast: " + broadcastMessage);
    }

    // Get min/max duration from line edits, ensuring min < max
    int minDuration = ui->LineEditPandemoniumDurationMin->text().toInt();
    int maxDuration = ui->LineEditPandemoniumDurationMax->text().toInt();

    if (minDuration > maxDuration) {
        std::swap(minDuration, maxDuration);
    }

    int durationMinutes = QRandomGenerator::global()->bounded(minDuration, maxDuration + 1);
    int durationMs = durationMinutes * 60 * 1000;

    QTimer::singleShot(durationMs, this, &MainWindow::runPandemoniumProtocol);
}
