// Copyright (c) 2015-2018, The Bytecoin developers, [ ] developers.
// Licensed under the GNU Lesser General Public License. See LICENSE for details.

#include <QFontDatabase>
#include <QDir>
#include <QLockFile>
//#include <QSettings>
#include <QFileDialog>
#include <QMetaEnum>
#include <QMessageBox>
#include <QAuthenticator>

#include "application.h"
#include "signalhandler.h"
#include "log/logger.h"
//#include "splashscreen.h"
#include "mainwindow.h"
#include "walletmodel.h"
#include "net/connectselectiondialog.h"
#include "settings.h"
#include "JsonRpc/JsonRpcClient.h"
#include "MiningManager.h"
#include "acc/addressbookmanager.h"

#include "changepassworddialog.h"
#include "askpassworddialog.h"
#include "crashdialog.h"
#include "importkeydialog.h"
#include "createproofdialog.h"
#include "checkproofdialog.h"
#include "walletdparamsdialog.h"

namespace WalletGUI {

WalletApplication::WalletApplication(int& argc, char** argv)
    : QApplication(argc, argv)
    , m_systemTrayIcon(new QSystemTrayIcon(this))
    , m_mainWindow(nullptr)
    , m_miningManager(nullptr)
    , addressBookManager_(nullptr)
    , walletd_(nullptr)
    , walletModel_(new WalletModel(this))
    , crashDialog_(new CrashDialog())
    , m_isAboutToQuit(false)
{
    setApplicationName("cryptonote"); // do not change becasuse it also changes data directory under Mac and Win
    setApplicationDisplayName(tr("Cryptonote Wallet"));
    setApplicationVersion("2.0.0");
    setQuitOnLastWindowClosed(false);
    QLocale::setDefault(QLocale::c());
    loadFonts();

    connect(this, &WalletApplication::createWalletdSignal, this, &WalletApplication::createWalletd, Qt::QueuedConnection);
}

WalletApplication::~WalletApplication()
{
    WalletLogger::deinit();
}

void WalletApplication::loadFonts()
{
    QFontDatabase::addApplicationFont(":font/Bold");
    QFontDatabase::addApplicationFont(":font/BoldItalic");
    QFontDatabase::addApplicationFont(":font/ExtraBold");
    QFontDatabase::addApplicationFont(":font/ExtraBoldItalic");
    QFontDatabase::addApplicationFont(":font/Italic");
    QFontDatabase::addApplicationFont(":font/Light");
    QFontDatabase::addApplicationFont(":font/LightItalic");
    QFontDatabase::addApplicationFont(":font/Regular");
    QFontDatabase::addApplicationFont(":font/Semibold");
    QFontDatabase::addApplicationFont(":font/SemiboldItalic");
    QFont font;
    font.setFamily("Open Sans");
    font.setStyleStrategy(QFont::PreferAntialias);
    QApplication::setFont(font);
}

bool WalletApplication::init()
{
    const QDir dataDir = Settings::instance().getDefaultWorkDir();
    makeDataDir(dataDir);
    const QDir logsDir = dataDir.absolutePath() + "/logs";
    makeDataDir(logsDir);
    WalletLogger::init(logsDir, true, this);
    WalletLogger::info(tr("[Application] Initializing..."));
    QString path = dataDir.absoluteFilePath("cryptonote-gui.lock");
    m_lockFile.reset(new QLockFile(path));

    if (!m_lockFile->tryLock())
    {
        WalletLogger::warning(tr("[Application] Cryptonote wallet GUI already running"));
        QMessageBox::warning(nullptr, QObject::tr("Fail"), tr("Cryptonote wallet GUI already running"));
        return false;
    }

    QObject::connect(&SignalHandler::instance(), &SignalHandler::quitSignal, this, &WalletApplication::quit);
//    initSystemTrayIcon();

    const bool connectionSelected = Settings::instance().connectionMethodSet();
    const bool walletFileSet = !Settings::instance().getWalletFile().isEmpty();
    const bool isFirstRun = !connectionSelected || (Settings::instance().getConnectionMethod() == ConnectionMethod::BUILTIN && !walletFileSet);
    WalletLogger::info(tr("[Application] Initialized successfully"));

    addressBookManager_ =  new AddressBookManager(this);
    m_miningManager = new MiningManager(this);

    QFile styleSheetFile(":style/qss");
    styleSheetFile.open(QIODevice::ReadOnly);
    const QByteArray styleSheet = styleSheetFile.readAll();
    styleSheetFile.close();
    WalletLogger::info(tr("[Application] Initialized successfully"));
    m_mainWindow = new MainWindow(walletModel_, styleSheet, m_miningManager, addressBookManager_, nullptr);
    m_mainWindow->show();
    connect(m_mainWindow, &MainWindow::createTxSignal, this, &WalletApplication::createTx);
    connect(m_mainWindow, &MainWindow::sendTxSignal, this, &WalletApplication::sendTx);
    connect(m_mainWindow, &MainWindow::restartDaemon, this, &WalletApplication::restartDaemon);
    connect(m_mainWindow, &MainWindow::createProofSignal, this, &WalletApplication::createProof);
    connect(m_mainWindow, &MainWindow::checkProofSignal, this, &WalletApplication::checkProof);
    connect(m_mainWindow, &MainWindow::showWalletdParamsSignal, this, &WalletApplication::showWalletdParams);

    connect(m_mainWindow, &MainWindow::createWalletSignal, this, &WalletApplication::createWallet);
    connect(m_mainWindow, &MainWindow::importKeysSignal, this, &WalletApplication::importKeys);
    connect(m_mainWindow, &MainWindow::openWalletSignal, this, &WalletApplication::openWallet);
    connect(m_mainWindow, &MainWindow::remoteWalletSignal, this, &WalletApplication::remoteWallet);
    connect(m_mainWindow, &MainWindow::encryptWalletSignal, this, &WalletApplication::encryptWallet);
    connect(this, &WalletApplication::builtinRunSignal, m_mainWindow, &MainWindow::builtinRun);
    connect(this, &WalletApplication::aboutToQuit, this, &WalletApplication::prepareToQuit);

    if(isFirstRun)
        firstRun();
    else
//        createWalletd();
        emit createWalletdSignal(QPrivateSignal{});
    return true;
}

void WalletApplication::subscribeToWalletd()
{
    connect(walletModel_, &WalletModel::getTransfersSignal, walletd_, &RemoteWalletd::getTransfers);
    connect(walletd_, &RemoteWalletd::statusReceivedSignal, walletModel_, &WalletModel::statusReceived);
    connect(walletd_, &RemoteWalletd::transfersReceivedSignal, walletModel_, &WalletModel::transfersReceived);
    connect(walletd_, &RemoteWalletd::addressesReceivedSignal, walletModel_, &WalletModel::addressesReceived);
    connect(walletd_, &RemoteWalletd::balanceReceivedSignal, walletModel_, &WalletModel::balanceReceived);
    connect(walletd_, &RemoteWalletd::viewKeyReceivedSignal, walletModel_, &WalletModel::viewKeyReceived);
    connect(walletd_, &RemoteWalletd::unspentsReceivedSignal, walletModel_, &WalletModel::unspentsReceived);
//    connect(walletd_, &RemoteWalletd::sendTxReceivedSignal, walletModel_, &WalletModel::transactionSent);
//    connect(walletd_, &RemoteWalletd::proofsReceivedSignal, this, &WalletApplication::showProof);
//    connect(walletd_, &RemoteWalletd::checkProofReceivedSignal, this, &WalletApplication::showCheckProof);

    connect(walletd_, &RemoteWalletd::stateChangedSignal, walletModel_, &WalletModel::stateChanged);
    connect(walletd_, &RemoteWalletd::connectedSignal, this, &WalletApplication::connectedToWalletd);
    connect(walletd_, &RemoteWalletd::networkErrorSignal, this, &WalletApplication::disconnectedFromWalletd);

    connect(walletd_, &RemoteWalletd::jsonErrorResponseSignal, m_mainWindow, &MainWindow::jsonErrorResponse);
    connect(walletd_, &RemoteWalletd::createTxReceivedSignal, m_mainWindow, &MainWindow::createTxReceived);
    connect(walletd_, &RemoteWalletd::connectedSignal, m_miningManager, &MiningManager::connectedToWalletd);
    connect(walletd_, &RemoteWalletd::networkErrorSignal, m_miningManager, &MiningManager::disconnectedFromWalletd);
    connect(walletd_, &RemoteWalletd::packetSent, m_mainWindow, &MainWindow::packetSent);
    connect(walletd_, &RemoteWalletd::packetReceived, m_mainWindow, &MainWindow::packetReceived);

    connect(walletd_, &RemoteWalletd::authRequiredSignal, this, &WalletApplication::requestWalletdAuth);
}

/* static */
void WalletApplication::makeDataDir(const QDir& dataDir)
{
    if (!dataDir.exists())
        dataDir.mkpath(dataDir.absolutePath());
}

void WalletApplication::initSystemTrayIcon()
{
    connect(m_systemTrayIcon, &QSystemTrayIcon::activated, this, &WalletApplication::trayActivated);
    m_systemTrayIcon->setIcon(QIcon(":images/cryptonote_lin"));
    m_systemTrayIcon->show();
}

void WalletApplication::dockClickHandler()
{
    if (m_isAboutToQuit)
        return;

    if (m_mainWindow != nullptr)
        m_mainWindow->show();
}

void WalletApplication::trayActivated(QSystemTrayIcon::ActivationReason /*reason*/)
{
    if (m_mainWindow != nullptr)
    {
        const Qt::WindowStates state = m_mainWindow->window()->windowState();
        m_mainWindow->window()->setWindowState(state & ~Qt::WindowMinimized);
        m_mainWindow->window()->show();
        m_mainWindow->raise();
        m_mainWindow->activateWindow();
    }
}

void WalletApplication::prepareToQuit()
{
    WalletLogger::debug(tr("[Application] Prepare to quit..."));

    m_isAboutToQuit = true;
    m_systemTrayIcon->hide();
    if (m_mainWindow)
    {
        m_mainWindow->close();
        m_mainWindow->deleteLater();
    }
    if (m_miningManager)
        m_miningManager->deleteLater();
    if (walletd_)
        walletd_->stop();
    WalletLogger::info(tr("[Application] Quit"));
    processEvents();
}

void WalletApplication::createTx(const RpcApi::CreateTransaction::Request& req)
{
    walletd_->createTx(req);
}

void WalletApplication::sendTx(const RpcApi::SendTransaction::Request& req)
{
    walletd_->sendTx(req);
}

void WalletApplication::sendCreateProof(const QString& txHash, const QString& message)
{
    const RpcApi::CreateSendProof::Request req{txHash, message, QStringList{}};
    walletd_->createProof(req);
}

void WalletApplication::sendCheckProof(const QString& proof)
{
    const RpcApi::CheckSendProof::Request req{proof};
    walletd_->checkSendProof(req);
}

void WalletApplication::createWalletd()
{
    if (Settings::instance().getConnectionMethod() == ConnectionMethod::BUILTIN)
    {
        const QString& walletFile = Settings::instance().getWalletFile();
        Q_ASSERT(!walletFile.isEmpty());
        runBuiltinWalletd(walletFile, false, QByteArray{});
    }
    else
        connectToRemoteWalletd();
}

void WalletApplication::restartDaemon()
{
//    createWalletd();
    emit createWalletdSignal(QPrivateSignal{});
}

void WalletApplication::splashMsg(const QString& msg)
{
    if (m_mainWindow)
        m_mainWindow->splashMsg(msg);
}

void WalletApplication::runBuiltinWalletd(const QString& walletFile, bool createNew, QByteArray&& keys)
{
    if (walletd_)
    {
        delete walletd_;
        walletd_ = nullptr;
    }

    splashMsg(tr("Running walletd..."));
    BuiltinWalletd* walletd = new BuiltinWalletd(walletFile, createNew, std::move(keys), this);
    walletd_ = walletd;

    connect(walletd, &BuiltinWalletd::daemonStandardOutputSignal, m_mainWindow, &MainWindow::addDaemonOutput);
    connect(walletd, &BuiltinWalletd::daemonStandardErrorSignal, m_mainWindow, &MainWindow::addDaemonError);
    connect(walletd, &BuiltinWalletd::daemonErrorOccurredSignal, this, &WalletApplication::daemonErrorOccurred);
    connect(walletd, &BuiltinWalletd::daemonFinishedSignal, this, &WalletApplication::daemonFinished);
    connect(walletd, &BuiltinWalletd::daemonFinishedSignal, this, &WalletApplication::detached);
    connect(walletd, &BuiltinWalletd::requestPasswordSignal, this, &WalletApplication::requestPassword);
    connect(walletd, &BuiltinWalletd::requestPasswordWithConfirmationSignal, this, &WalletApplication::requestPasswordWithConfirmation);

    subscribeToWalletd();
    Settings::instance().setConnectionMethod(ConnectionMethod::BUILTIN);
    Settings::instance().setWalletFile(walletFile);

    connect(walletd, &BuiltinWalletd::connectedSignal, this, &WalletApplication::builtinRunSignal);

    walletd_->run();
}

void WalletApplication::connectedToWalletd()
{
    crashDialog_->accept();
    if (m_mainWindow)
        m_mainWindow->setConnectedState();
}

void WalletApplication::disconnectedFromWalletd()
{
    if (m_mainWindow)
        m_mainWindow->setDisconnectedState();
}

void WalletApplication::detached()
{
    splashMsg(tr("Cryptonote GUI is in detached state.\nYou can open a wallet file, create a new one, or connect to remote walletd daemon."));
    if (m_mainWindow)
        m_mainWindow->setDisconnectedState();
}

void WalletApplication::firstRun()
{
    splashMsg(tr("Looks like this is your first run of the new Cryptonote Wallet GUI. It is in detached state now.\nYou can open a wallet file, create a new one, or connect to remote walletd daemon."));
}

void WalletApplication::daemonErrorOccurred(QProcess::ProcessError error, QString errorString)
{
    if (error != QProcess::FailedToStart)
        return;

    m_mainWindow->showLog();
    if (crashDialog_->execWithReason(tr("Failed to run walletd: ") + errorString, false) == QDialog::Accepted)
    {
        splashMsg(tr("Restarting walletd..."));
        walletd_->run();
    }
}

void WalletApplication::daemonFinished(int exitCode, QProcess::ExitStatus /*exitStatus*/)
{
    BuiltinWalletd* walletd = static_cast<BuiltinWalletd*>(walletd_);
    if (walletd->getState() == BuiltinWalletd::State::STOPPED)
        return;

    const QMetaEnum metaEnum = QMetaEnum::fromType<BuiltinWalletd::ReturnCode>();
    qDebug("[WalletApplication] Daemon finished. Return code: %s (%d)",
                metaEnum.valueToKey(static_cast<int>(exitCode)),
                exitCode);
//    const QString msg = metaEnum.valueToKey(static_cast<int>(exitCode));
    bool showPasswordEdit = false;
    QString msg;
    switch(exitCode)
    {
    case static_cast<int>(BuiltinWalletd::ReturnCode::CRYPTONOTED_DATABASE_ERROR):
        msg = tr("Database write error. Disk is full or database is corrupted.");
        break;
    case static_cast<int>(BuiltinWalletd::ReturnCode::CRYPTONOTED_ALREADY_RUNNING):
        msg = tr("Cannot run cryptonoted. Another instance of cryptonoted is running.");
        break;
    case static_cast<int>(BuiltinWalletd::ReturnCode::WALLETD_BIND_PORT_IN_USE):
        msg = tr("Cannot run walletd. Walletd bind port in use.");
        break;
    case static_cast<int>(BuiltinWalletd::ReturnCode::CRYPTONOTED_BIND_PORT_IN_USE):
        msg = tr("Cannot run cryptonoted. Cryptonoted bind port in use.");
        break;
    case static_cast<int>(BuiltinWalletd::ReturnCode::WALLET_FILE_READ_ERROR):
        msg = tr("Cannot read the specified wallet file.");
        break;
    case static_cast<int>(BuiltinWalletd::ReturnCode::WALLET_FILE_UNKNOWN_VERSION):
        msg = tr("Version of the specified wallet file is unknown.");
        break;
    case static_cast<int>(BuiltinWalletd::ReturnCode::WALLET_FILE_DECRYPT_ERROR):
        msg = tr("Cannot decrypt the wallet file. The specified password is incorrect or the wallet file is corrupted.");
//        showPasswordEdit = true;
        break;
    case static_cast<int>(BuiltinWalletd::ReturnCode::WALLET_FILE_WRITE_ERROR):
        msg = tr("Cannot write to the wallet file. Probably your file system is read only.");
        break;
    case static_cast<int>(BuiltinWalletd::ReturnCode::WALLET_FILE_EXISTS):
        msg = tr("The specified wallet file already exists. Cryptonote wallet could not overwrite an existed file for safety reason. If you want to overwrite the file please remove it manually and try again.");
        break;
    case static_cast<int>(BuiltinWalletd::ReturnCode::WALLET_WITH_THE_SAME_VIEWKEY_IN_USE):
        msg = tr("Another walletd instance is using the specified wallet file or another wallet file with the same view key.");
        break;
    case static_cast<int>(BuiltinWalletd::ReturnCode::WALLETD_WRONG_ARGS):
        msg = tr("Wrong arguments passed to walletd.");
        break;
    default:
        msg = tr("Walletd just crashed. %1. Return code %2. ").arg(walletd->errorString()).arg(exitCode);
        break;
    }

    if (crashDialog_->execWithReason(msg, showPasswordEdit) == QDialog::Accepted)
    {
//        if (showPasswordEdit)
//            walletd->setPassword(crashDialog_->getPassword());
//        walletd->run();
        restartDaemon();
    }
}

void WalletApplication::connectToRemoteWalletd()
{
    if (walletd_)
    {
        delete walletd_;
        walletd_ = nullptr;
    }

    splashMsg(tr("Connecting to walletd..."));
    walletd_ = new RemoteWalletd(Settings::instance().getRpcEndPoint(), this);
    subscribeToWalletd();
    Settings::instance().setConnectionMethod(ConnectionMethod::REMOTE);

    connect(walletd_, &RemoteWalletd::networkErrorSignal, this, &WalletApplication::detached);
    connect(walletd_, &RemoteWalletd::connectedSignal, this, &WalletApplication::remoteConnectedSignal);

    walletd_->run();
}

void WalletApplication::createWallet(QWidget* parent)
{
    const QString fileName = QFileDialog::getSaveFileName(
                parent,
                tr("Create wallet file"),
                QDir::homePath(),
                tr("Wallet files (*.wallet);;All files (*)"));
    if (fileName.isEmpty())
        return;

    runBuiltinWalletd(fileName, true, QByteArray{});
}

void WalletApplication::openWallet(QWidget* parent)
{
    const QString fileName = QFileDialog::getOpenFileName(
                parent,
                tr("Open wallet file"),
                QDir::homePath(),
                tr("Wallet files (*.wallet);;All files (*)"));
    if (fileName.isEmpty())
        return;

    runBuiltinWalletd(fileName, false, QByteArray{});
}

void WalletApplication::remoteWallet(QWidget* parent)
{
    ConnectSelectionDialog dlg(parent);
    if (dlg.exec() != QDialog::Accepted)
        return;

    connectToRemoteWalletd();
}

void WalletApplication::encryptWallet(QWidget *parent)
{
    ChangePasswordDialog dlg(true, parent);
    if (dlg.exec() != QDialog::Accepted)
        return;

    BuiltinWalletd* walletd = static_cast<BuiltinWalletd*>(walletd_);
    walletd->changeWalletPassword(dlg.getOldPassword(), dlg.getNewPassword());
}

void WalletApplication::importKeys(QWidget* parent)
{
    ImportKeyDialog dlg(parent);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString fileName = QFileDialog::getSaveFileName(
                parent,
                tr("Create wallet file"),
                QDir::homePath(),
                tr("Wallet files (*.wallet);;All files (*)"));
    if (fileName.isEmpty())
        return;

    QByteArray keys = dlg.getKey();
    runBuiltinWalletd(fileName, true, std::move(keys));
}

void WalletApplication::requestPassword()
{
    AskPasswordDialog dlg(false, m_mainWindow);
    BuiltinWalletd* walletd = static_cast<BuiltinWalletd*>(walletd_);
    connect(walletd, &BuiltinWalletd::daemonErrorOccurredSignal, &dlg, &AskPasswordDialog::reject);
    if (dlg.exec() == QDialog::Accepted)
        walletd->setPassword(dlg.getPassword());
    else
        walletd_->stop();
}

void WalletApplication::requestPasswordWithConfirmation()
{
    ChangePasswordDialog dlg(false, m_mainWindow);
    BuiltinWalletd* walletd = static_cast<BuiltinWalletd*>(walletd_);
    connect(walletd, &BuiltinWalletd::daemonErrorOccurredSignal, &dlg, &ChangePasswordDialog::reject);
    if (dlg.exec() == QDialog::Accepted)
        walletd->setPassword(dlg.getNewPassword());
    else
        walletd_->stop();
}

void WalletApplication::requestWalletdAuth(QAuthenticator* authenticator)
{
    AskPasswordDialog dlg(true, m_mainWindow);
    if (dlg.exec() == QDialog::Accepted)
    {
        authenticator->setUser(dlg.getUser());
        authenticator->setPassword(dlg.getPassword());
    }
    else
    {
        walletd_->stop();
        detached();
    }
}

void WalletApplication::createProof(const QString& txHash)
{
    CreateProofDialog dlg{txHash, m_mainWindow};
    connect(&dlg, &CreateProofDialog::generateProofSignal, this, &WalletApplication::sendCreateProof);
    connect(walletd_, &RemoteWalletd::proofsReceivedSignal,
            &dlg,
            [&dlg](const RpcApi::Proofs& proofs)
            {
                dlg.addProofs(proofs.send_proofs);
            });
    dlg.exec();
}

void WalletApplication::checkProof()
{
    CheckProofDialog dlg{m_mainWindow};
    connect(&dlg, &CheckProofDialog::checkProofSignal, this, &WalletApplication::sendCheckProof);
    connect(walletd_, &RemoteWalletd::checkProofReceivedSignal,
            &dlg,
            [&dlg](const RpcApi::ProofCheck& check)
            {
                dlg.showCheckResult(check.validation_error);
            });
    dlg.exec();
}

void WalletApplication::showWalletdParams()
{
    WalletdParamsDialog dlg(Settings::instance().getConnectionMethod() == ConnectionMethod::BUILTIN, m_mainWindow);
    connect(&dlg, &WalletdParamsDialog::restartWalletd, this, &WalletApplication::restartDaemon);
    BuiltinWalletd* walletd = static_cast<BuiltinWalletd*>(walletd_);
    connect(walletd, &BuiltinWalletd::daemonErrorOccurredSignal, &dlg, &WalletdParamsDialog::reject);
    dlg.exec();
}

}
