#ifndef VOICEMANAGER_H
#define VOICEMANAGER_H

#include <QObject>
#include <QUdpSocket>
#include <QMutex>
#include <QTimer>
#include <QQueue>
#include <QByteArray>
#include "portaudio.h"
#include<QThread>
struct VoiceData {
    QUdpSocket* udpSocket = nullptr;
    QString senderId;
    QString receiverId;
    QString udpServerIp;
    quint16 udpServerPort = 0;
    QMutex* mutex = nullptr;
    bool isVoiceCalling = false;
    class VoiceManager *managerInstance = nullptr;
};

class VoiceManager : public QObject
{
    Q_OBJECT
public:
    explicit VoiceManager(QObject *parent = nullptr);
    ~VoiceManager() override;

    void setSenderId(const QString &senderId);
    void setReceiverId(const QString &receiverId);
    bool startVoiceCall();
    void stopVoiceCall();
    bool isVoiceCalling() ;

signals:
    void voiceCallStarted();
    void voiceCallStopped();
    void errorOccurred(const QString &errorMsg);
    // 新增：触发子线程内初始化定时器
    void sigInitTimers();
    // 新增：触发子线程内启动/停止定时器
    void sigStartTimers();
    void sigStopTimers();

private slots:
    void onUdpReadyRead();
    void sendCachedAudioData();
    void checkAudioStreamStatus();
    // 新增：子线程内初始化定时器的槽函数
    void slotInitTimers();
    // 新增：子线程内启动/停止定时器的槽函数
    void slotStartTimers();
    void slotStopTimers();

private:
    double m_currentSampleRate = 16000;    // 当前使用的采样率（默认16000）
    unsigned long m_currentFrameSize = 256;// 当前缓冲区帧数（默认256）
    QByteArray m_remainingAudioData;

    bool initPortAudio();
    bool startAudioStreams();
    void stopAudioStreams();

    static int paInputCallback(const void* inputBuffer, void* outputBuffer,
                               unsigned long framesPerBuffer,
                               const PaStreamCallbackTimeInfo* timeInfo,
                               PaStreamCallbackFlags statusFlags,
                               void* userData);
    static int paOutputCallback(const void* inputBuffer, void* outputBuffer,
                                unsigned long framesPerBuffer,
                                const PaStreamCallbackTimeInfo* timeInfo,
                                PaStreamCallbackFlags statusFlags,
                                void* userData);

    // 常量配置
    const QString m_udpServerIp = "192.168.130.128";
    const quint16 m_udpServerPort = 8889;
    const quint16 m_localUdpPort = 8890;

    // 核心成员（定时器先声明，不初始化）
    QString m_senderId;
    QString m_receiverId;
    bool m_isVoiceCalling = false;
    QMutex m_voiceMutex;
    QMutex m_queueMutex;
    QUdpSocket *m_udpSocket = nullptr;
    PaStream *m_inputStream = nullptr;
    PaStream *m_outputStream = nullptr;
    // 定时器指针（初始为null，子线程内创建）
    QTimer* m_streamCheckTimer = nullptr;
    QTimer* m_sendAudioTimer = nullptr;
    QQueue<QByteArray> m_audioDataQueue;
    VoiceData m_voiceData;
};

#endif // VOICEMANAGER_H
