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

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":data/icon.png"));

    QSystemSemaphore sem( "firgui_update", 1, QSystemSemaphore::Open );
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
    progress.setWindowTitle("FIR Controller");
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setWindowFlags(Qt::Dialog | Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
    progress.setCancelButton(nullptr);

    progress.setValue(0);

    QFile updateStatusFile("updatestatus.txt");
    if(!updateStatusFile.open(QFile::WriteOnly)){
        qCritical()<< "Could not open file for writing update status.";
        QMessageBox::critical(nullptr, "Fir Controller", "Could not apply update."); return -1;
    }
    QTextStream updateStatus(&updateStatusFile);
    updateStatus << "fail";

    progress.setValue(5);

    QDir cdir;
    if(!cdir.exists("update")){
        qCritical() << "Update directory does not exist.";
        QMessageBox::critical(nullptr, "Fir Controller", "Could not apply update."); return -1;
    }
    if(!cdir.exists("update/updater/updater.exe")){
        qCritical() << "Updater executable is not in place.";
        QMessageBox::critical(nullptr, "Fir Controller", "Could not apply update."); return -1;
    }
    if(!cdir.exists("update/app")){
        qCritical() << "Application directory does not exist";
        QMessageBox::critical(nullptr, "Fir Controller", "Could not apply update."); return -1;
    }

    progress.setValue(10);

    //cleanup
    if(cdir.exists("backup")){
        qInfo() << "Deleting old backup.";
        QDir bkdir(cdir); bkdir.cd("backup");
        if(!bkdir.removeRecursively()){
            qInfo() << "Could not remove old backup.";
            QMessageBox::critical(nullptr, "Fir Controller", "Could not apply update."); return -1;
        }
    }

    auto mainDirContent = cdir.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
    mainDirContent.removeOne("data");
    mainDirContent.removeOne("update");
    mainDirContent.removeOne("updatelog.txt");
    mainDirContent.removeOne("updatestatus.txt");
    cdir.mkdir("backup");
    {
        int i = 0; int s = mainDirContent.length();
        for(auto it : mainDirContent){
            qInfo() << "Moved" << it <<QFile::rename(it, "backup/" + it);
            progress.setValue(10+(40*(++i/s)));
        }
    }
    QDir appdir(cdir);
    if(!appdir.cd("update/app")){
        qCritical() << "Could not cd to update/app.";
        QMessageBox::critical(nullptr, "Fir Controller", "Could not apply update."); return -1;
    }
    auto appDirContent = appdir.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
    {
        int i = 0; int s = mainDirContent.length();
        for(auto it : appDirContent){
            qInfo() << "Moved" << it <<QFile::rename("update/app/" + it,  it);
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
    QThread::sleep(1);
    progress.setValue(100);
    qInfo() << "End.";
    return 0;
}









