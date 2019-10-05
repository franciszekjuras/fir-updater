#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QSystemSemaphore>
#include <QThread>
#include <QProgressDialog>
#include <QMessageBox>
#include <QIcon>
#include <QProcess>

#include <fstream>

std::ofstream logfile;

void logToFile(QtMsgType type, const QMessageLogContext &context, const QString &msg){
    QString datetime = QDateTime::currentDateTime().toString("hh:mm:ss");
    switch (type) {
    case QtDebugMsg:
        logfile << datetime.toStdString() << " [Debug] " << msg.toStdString() << "\n";
        break;
    case QtCriticalMsg:
        logfile << datetime.toStdString() << " [Critical] " << msg.toStdString() << "\n";
        break;
    case QtWarningMsg:
        logfile << datetime.toStdString()<< " [Warning] " << msg.toStdString() << "\n";
        break;
    case QtInfoMsg:
        logfile << datetime.toStdString() <<  " [Info] " << msg.toStdString() << "\n";
        break;
    case QtFatalMsg:
        logfile << datetime.toStdString() <<  " [Fatal] " << msg.toStdString() << "\n";
        logfile.flush();
        abort();
  }
  logfile.flush();
}

void failureMessage(){
    QMessageBox::critical(nullptr, APPNAME, "Could not apply update.");
}

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":data/icon.png"));

    QSystemSemaphore sem( APPSEMAPHORE, 1, QSystemSemaphore::Open );
    sem.acquire();


    //Just for testing
//    QDir wdir;
//    wdir.cd("aaa");
//    qDebug() << "Working directory changed:" << QDir::setCurrent(wdir.absolutePath());
    //End

    logfile.open("updatelog.txt", std::ofstream::out);
    if (logfile)
        qInstallMessageHandler(logToFile);
    qInfo() << "Start.";

    QProgressDialog progress("Applying update.", "", 0, 100);
    progress.setWindowTitle(APPNAME);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setWindowFlags(Qt::Dialog | Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
    progress.setCancelButton(nullptr);

    progress.setValue(0);

    QFile updateStatusFile("updatestatus");
    if(!updateStatusFile.open(QFile::WriteOnly)){
        qCritical()<< "Could not open file for writing update status.";
        failureMessage(); return -1;
    }
    QTextStream updateStatus(&updateStatusFile);
    updateStatus << "fail";

#ifdef _WIN32
    QFile exeFile(QString(APPEXE)+".exe");
#else
    QFile exeFile(APPEXE);
#endif
    if(!exeFile.open(QIODevice::ReadWrite | QIODevice::ExistingOnly)){
        qCritical() << "Could not open executable file with read and write rights.";
        QMessageBox::critical(nullptr, "FIR Controller", "Could not apply update because application files are in use.");
        return -1;
    }
    exeFile.close();

    QDir cdir;
    if(!cdir.exists("update")){
        qCritical() << "Update directory does not exist.";
        failureMessage(); return -1;
    }
    if(!cdir.exists("update/updater/updater.exe")){
        qCritical() << "Updater executable is not in place.";
        failureMessage(); return -1;
    }
    if(!cdir.exists("update/app")){
        qCritical() << "Application directory does not exist";
        failureMessage(); return -1;
    }
#ifdef _WIN32
    if(!cdir.exists(QString("update/app/") + APPEXE +".exe")){
#else
    if(!cdir.exists(QString("update/app/") + APPEXE)){
#endif
        qCritical() << "Application executable does not exist";
        failureMessage(); return -1;
    }

    progress.setValue(10);

    //cleanup
    if(cdir.exists("backup")){
        qInfo() << "Deleting old backup.";
        QDir bkdir(cdir); bkdir.cd("backup");
        if(!bkdir.removeRecursively()){
            qCritical() << "Could not remove old backup.";
            failureMessage(); return -1;
        }
    }

    auto mainDirContent = cdir.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
    mainDirContent.removeOne("data");
    mainDirContent.removeOne("update");
    mainDirContent.removeOne("updatelog.txt");
    mainDirContent.removeOne("updatestatus");
    cdir.mkdir("backup");
    {
        int i = 0; int s = mainDirContent.length();
        for(auto it : mainDirContent){
            bool success = QFile::rename(it, "backup/" + it);
            qInfo() << "Moved" << it << success;
            if(!success){
                 for(auto it2 : mainDirContent){
                     if(it2 == it) break;
                     success = QFile::rename("backup/" + it, it);
                     qInfo() << "Moved back" << "backup/" + it << success;
                     if(!success){
                         qCritical() << "Could not apply backup. Application files corrupted.";
                         QMessageBox::critical(nullptr, APPNAME, QString("Update corrupted application files. Download app manually from ") + APPSOURCE);
                         return -1;
                     }
                 }
                 //update failed but changes reverted
                 qCritical() << "Could not remove application files. Original state recovered from backup.";
                 QMessageBox::critical(nullptr, APPNAME, "Could not apply update because application files are in use or updater has no rights to move them.");
                 return -1;
            }
            progress.setValue(10+(40*(++i/s)));
        }
    }
    QDir appdir(cdir);
    if(!appdir.cd("update/app")){
        qCritical() << "Could not cd to update/app.";
        failureMessage(); return -1;
    }
    auto appDirContent = appdir.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
    {
        int i = 0; int s = appDirContent.length();
        for(auto it : appDirContent){
            bool success = QFile::rename("update/app/" + it,  it);
            qInfo() << "Moved" << it << success;
            if(!success){
                for(auto it2 : appDirContent){
                    if(it2 == it) break;
                    success = QFile::rename(it, "update/app/" + it);
                    qInfo() << "Moved back" << it << success;
                    if(!success){
                        qCritical() << "Could not revert update. Application files corrupted.";
                        QMessageBox::critical(nullptr, APPNAME, "Update corrupted application files. Download app manually from https://github.com/franciszekjuras/firgui/releases/latest");
                        return -1;
                    }
                }
                for(auto it3 : mainDirContent){
                    success = QFile::rename("backup/" + it, it);
                    qInfo() << "Moved back" << "backup/" +  it << success;
                    if(!success){
                        qCritical() << "Could not apply backup. Application files corrupted.";
                        QMessageBox::critical(nullptr, APPNAME, "Update corrupted application files. Download app manually from https://github.com/franciszekjuras/firgui/releases/latest");
                        return -1;
                    }
                }
                qCritical() << "Could not move update files. Original state recovered from backup.";
                QMessageBox::critical(nullptr, APPNAME, "Could not apply update because update files are in use or updater has no rights to move them.");
                return -1;
            }
            progress.setValue(50+(49*(++i/s)));
        }
    }
    appdir.removeRecursively();

    //if everything goes well

    QDir bkdir(cdir);
    if(bkdir.cd("backup"))
        bkdir.removeRecursively();

    updateStatus.seek(0);
    updateStatusFile.resize(0);
    updateStatus << "ok";
    updateStatus.flush();updateStatusFile.close();
    QThread::sleep(1);
    progress.setValue(100);
#ifdef _WIN32
    if(cdir.exists(QString(APPEXE)+".exe")){
        if(!QProcess::startDetached(QString("\"") + APPEXE +".exe\"")){
#else
    if(cdir.exists(APPEXE)){
        if(!QProcess::startDetached(QString("\"") + APPEXE +"\"")){
#endif
            qWarning() << "Could not restart application";
        }
    } else{
        qWarning() << "Could not find application executable";
    }
    qInfo() << "End.";
    return 0;
}









