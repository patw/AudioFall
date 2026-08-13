#include "wav.h"
#include "pipeline.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QtEndian>

class AudioFallTests : public QObject {
 Q_OBJECT
 QByteArray pcm(const QList<QPair<double,qint16>> &segments) { QByteArray out; for(auto s:segments)for(int i=0;i<qRound(s.first*16000);++i){char b[2];qToLittleEndian(s.second,reinterpret_cast<uchar*>(b));out.append(b,2);}return out; }
 int frames(const QString &path) { QFile f(path);f.open(QIODevice::ReadOnly);QByteArray b=f.readAll();return qFromLittleEndian<quint32>((const uchar*)b.constData()+40)/2; }
private slots:
 void longSilenceIsRemoved() { QTemporaryDir dir;QString p=dir.filePath("meeting.wav"),error;QVERIFY(Wav::writePcm16Mono(p,pcm({{1,4000},{.25,0},{1,4000},{2,0},{1,4000}}),16000,&error));QVERIFY2(Wav::removeSilence(p,-40,1,.1,&error),qPrintable(error));QVERIFY(qAbs(frames(p)/16000.0-3.45)<.08); }
 void shortPauseIsPreserved() { QTemporaryDir dir;QString p=dir.filePath("meeting.wav"),error;QByteArray source=pcm({{1,4000},{.2,0},{1,4000}});QVERIFY(Wav::writePcm16Mono(p,source,16000,&error));QVERIFY(Wav::removeSilence(p,-40,1,.15,&error));QFile f(p);f.open(QIODevice::ReadOnly);QVERIFY(f.readAll().endsWith(source)); }
 void invalidParametersFail() { QTemporaryDir dir;QString p=dir.filePath("meeting.wav"),error;QVERIFY(Wav::writePcm16Mono(p,pcm({{1,4000}}),16000,&error));QVERIFY(!Wav::removeSilence(p,std::numeric_limits<double>::quiet_NaN(),1,.15,&error));QVERIFY(!error.isEmpty()); }
};
QTEST_MAIN(AudioFallTests)
#include "tests.moc"
