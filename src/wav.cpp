#include "wav.h"
#include <QFile>
#include <QSaveFile>
#include <QtEndian>
#include <cmath>

namespace {
struct Parsed { int rate=0; int channels=0; int bits=0; QByteArray data; };
void put32(QByteArray &out, quint32 v) { char b[4]; qToLittleEndian(v,reinterpret_cast<uchar*>(b)); out.append(b,4); }
void put16(QByteArray &out, quint16 v) { char b[2]; qToLittleEndian(v,reinterpret_cast<uchar*>(b)); out.append(b,2); }
bool parse(const QString &path, Parsed &p, QString *err) {
 QFile f(path); if(!f.open(QIODevice::ReadOnly)){if(err)*err=f.errorString();return false;} QByteArray b=f.readAll();
 if(b.size()<44||b.left(4)!="RIFF"||b.mid(8,4)!="WAVE"){if(err)*err="Not a RIFF/WAV file";return false;}
 int at=12; bool fmt=false,data=false; while(at+8<=b.size()) { QByteArray id=b.mid(at,4); quint32 n=qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(b.constData()+at+4)); at+=8; if(at+int(n)>b.size()){if(err)*err="Malformed WAV chunk";return false;} if(id=="fmt "&&n>=16){ const char *x=b.constData()+at; if(qFromLittleEndian<quint16>((const uchar*)x)!=1){if(err)*err="Only PCM WAV is supported";return false;} p.channels=qFromLittleEndian<quint16>((const uchar*)x+2);p.rate=qFromLittleEndian<quint32>((const uchar*)x+4);p.bits=qFromLittleEndian<quint16>((const uchar*)x+14);fmt=true;} if(id=="data"){p.data=b.mid(at,n);data=true;} at+=int(n)+(n&1); }
 if(!fmt||!data){if(err)*err="WAV needs fmt and data chunks";return false;} return true;
}
}
bool Wav::writePcm16Mono(const QString &path,const QByteArray &pcm,int rate,QString *err) {
 if(rate<=0||pcm.size()%2){if(err)*err="Invalid PCM payload";return false;} QByteArray h("RIFF",4); put32(h,36+pcm.size()); h.append("WAVEfmt ",8);put32(h,16);put16(h,1);put16(h,1);put32(h,rate);put32(h,rate*2);put16(h,2);put16(h,16);h.append("data",4);put32(h,pcm.size());
 QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)){if(err)*err=f.errorString();return false;}f.write(h);f.write(pcm);if(!f.commit()){if(err)*err=f.errorString();return false;}return true;
}
bool Wav::removeSilence(const QString &inputPath,const QString &outputPath,double thresholdDb,double minSilence,double padding,QString *err) {
 if(!std::isfinite(thresholdDb)||minSilence<0||padding<0){if(err)*err="Invalid silence-removal parameters";return false;}
 Parsed p;if(!parse(inputPath,p,err))return false;
 if(p.channels!=1||p.bits!=16||p.rate<=0){if(err)*err="Silence removal requires mono, 16-bit PCM WAV";return false;}

 const int count=p.data.size()/2, window=qMax(1,qRound(p.rate*.025)), pad=qRound(padding*p.rate);
 const double threshold=32767.0*std::pow(10.0,thresholdDb/20.0);

 // Decode to samples once for index-based access.
 QVector<qint16> samples(count);
 for(int i=0;i<count;++i)
   samples[i]=qFromLittleEndian<qint16>(reinterpret_cast<const uchar*>(p.data.constData())+i*2);

 // Mark each analysis window loud/quiet by RMS.
 QVector<bool> loud;
 for(int start=0;start<count;start+=window){
   int end=qMin(count,start+window);
   double sum=0;
   for(int i=start;i<end;++i) sum+=double(samples[i])*samples[i];
   loud.append(std::sqrt(sum/(end-start))>=threshold);
 }

 // Build speech regions; a region closes after minSilence seconds of quiet.
 QVector<QPair<int,int>> regions;
 int speech=-1,quiet=-1;
 for(int i=0;i<loud.size();++i){
   int start=i*window,end=qMin(count,start+window);
   if(loud[i]){if(speech<0)speech=start;quiet=-1;}
   else if(speech>=0){if(quiet<0)quiet=start;if(end-quiet>=minSilence*p.rate){regions.append({qMax(0,speech-pad),qMin(count,quiet+pad)});speech=-1;quiet=-1;}}
 }
 if(speech>=0)regions.append({qMax(0,speech-pad),count});

 // Nothing to remove: leave the source untouched and produce an identical copy.
 if(regions.empty())return writePcm16Mono(outputPath,p.data,p.rate,err);

 QVector<QPair<int,int>> merged;
 for(auto r:regions){if(!merged.empty()&&r.first<=merged.last().second)merged.last().second=qMax(merged.last().second,r.second);else merged.append(r);}

 // Splice kept regions with a short linear crossfade so joins don't click/pop.
 const int fade=qMax(1,qMin(qRound(p.rate*.005),qMax(1,window/2)));
 QVector<qint16> out;
 for(const auto &r:merged){
   const int start=r.first,end=r.second;
   if(out.isEmpty()){for(int i=start;i<end;++i)out.append(samples[i]);continue;}
   const int overlap=qMin(fade,qMin(out.size(),end-start));
   for(int j=0;j<overlap;++j){
     const double t=double(j+1)/double(overlap+1);
     const int idx=out.size()-overlap+j;
     out[idx]=qint16(qRound((1.0-t)*out[idx]+t*samples[start+j]));
   }
   for(int i=start+overlap;i<end;++i)out.append(samples[i]);
 }

 QByteArray pcm;
 pcm.reserve(out.size()*2);
 for(qint16 s:out){char b[2];qToLittleEndian(s,reinterpret_cast<uchar*>(b));pcm.append(b,2);}
 return writePcm16Mono(outputPath,pcm,p.rate,err);
}
