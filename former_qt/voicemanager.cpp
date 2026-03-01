#include "voicemanager.h"
#include <QDebug>
#include <QHostAddress>
#include <cstring>
#include <QThread>

VoiceManager::VoiceManager(QObject *parent)
    : QObject{parent}
{
    // 1. 初始化UDP（子线程内执行，因为VoiceManager会被moveToThread）
    m_udpSocket = new QUdpSocket(this);
    if (!m_udpSocket->bind(QHostAddress::Any, m_localUdpPort)) {
        qCritical() << "UDP绑定失败：" << m_udpSocket->errorString();
        emit errorOccurred(QString("UDP绑定失败：%1").arg(m_udpSocket->errorString()));
    } else {
        connect(m_udpSocket, &QUdpSocket::readyRead, this, &VoiceManager::onUdpReadyRead);
        qDebug() << "[VoiceManager] UDP初始化成功，端口：" << m_localUdpPort;
    }

    // 2. 初始化PortAudio
    if (!initPortAudio()) {
        emit errorOccurred("PortAudio初始化失败，语音功能不可用");
    }

    // 3. 初始化回调参数
    m_voiceData.udpSocket = m_udpSocket;
    m_voiceData.senderId = m_senderId;
    m_voiceData.receiverId = m_receiverId;
    m_voiceData.udpServerIp = m_udpServerIp;
    m_voiceData.udpServerPort = m_udpServerPort;
    m_voiceData.mutex = &m_voiceMutex;
    m_voiceData.isVoiceCalling = m_isVoiceCalling;
    m_voiceData.managerInstance = this;

    // 4. 关键：连接信号-槽，确保定时器操作在子线程执行
    // 初始化定时器（QueuedConnection确保在子线程执行）
    connect(this, &VoiceManager::sigInitTimers, this, &VoiceManager::slotInitTimers, Qt::QueuedConnection);
    // 启动/停止定时器
    connect(this, &VoiceManager::sigStartTimers, this, &VoiceManager::slotStartTimers, Qt::QueuedConnection);
    connect(this, &VoiceManager::sigStopTimers, this, &VoiceManager::slotStopTimers, Qt::QueuedConnection);

    // 5. 触发子线程内初始化定时器（此时VoiceManager已moveToThread，槽函数在子线程执行）
    emit sigInitTimers();
}

VoiceManager::~VoiceManager()
{
    stopVoiceCall();
    Pa_Terminate();
    if (m_udpSocket) m_udpSocket->abort();
    qDebug() << "[VoiceManager] 资源已释放";
}

// 新增：子线程内初始化定时器（创建+配置，不启动）
void VoiceManager::slotInitTimers()
{
    // 此时执行线程是VoiceManager所在的子线程！
    qDebug() << "[VoiceManager] 子线程内初始化定时器，线程ID：" << QThread::currentThreadId();

    // 创建流状态检测定时器（子线程内创建）
    m_streamCheckTimer = new QTimer(this);
    m_streamCheckTimer->setInterval(200);
    connect(m_streamCheckTimer, &QTimer::timeout, this, &VoiceManager::checkAudioStreamStatus);

    // 创建音频发送定时器（子线程内创建）
    m_sendAudioTimer = new QTimer(this);
    m_sendAudioTimer->setInterval(20); // 初始值，启动时动态调整
    connect(m_sendAudioTimer, &QTimer::timeout, this, &VoiceManager::sendCachedAudioData);
}

// 新增：子线程内启动定时器
void VoiceManager::slotStartTimers()
{
    qDebug() << "[VoiceManager] 子线程内启动定时器，线程ID：" << QThread::currentThreadId();
    if (m_streamCheckTimer) m_streamCheckTimer->start();
    if (m_sendAudioTimer) m_sendAudioTimer->start();
}

// 新增：子线程内停止定时器
void VoiceManager::slotStopTimers()
{
    qDebug() << "[VoiceManager] 子线程内停止定时器，线程ID：" << QThread::currentThreadId();
    if (m_streamCheckTimer) m_streamCheckTimer->stop();
    if (m_sendAudioTimer) m_sendAudioTimer->stop();
}

void VoiceManager::setSenderId(const QString &senderId)
{
    QMutexLocker locker(&m_voiceMutex);
    m_senderId = senderId;
    m_voiceData.senderId = senderId;
}

void VoiceManager::setReceiverId(const QString &receiverId)
{
    QMutexLocker locker(&m_voiceMutex);
    m_receiverId = receiverId;
    m_voiceData.receiverId = receiverId;
}

// 1. 修改 startVoiceCall 函数，增加启动过程日志
bool VoiceManager::startVoiceCall()
{
    QMutexLocker locker(&m_voiceMutex);
    qDebug() << "[VoiceManager] 尝试启动语音通话，当前状态：" << m_isVoiceCalling
             << "senderId：" << m_senderId << "receiverId：" << m_receiverId;

    if (m_senderId.isEmpty() || m_receiverId.isEmpty()) {
        emit errorOccurred("未设置发送/接收方ID");
        qCritical() << "[VoiceManager] 启动失败：发送/接收方ID为空";
        return false;
    }
    if (m_isVoiceCalling) {
        qDebug() << "[VoiceManager] 启动失败：已在通话中";
        return true;
    }

    m_isVoiceCalling = true;
    m_voiceData.isVoiceCalling = true;
    locker.unlock();

    qDebug() << "[VoiceManager] 开始启动音频流...";
    if (startAudioStreams()) {
        emit sigStartTimers(); // 启动定时器
        emit voiceCallStarted();
        qDebug() << "[VoiceManager] 语音通话启动成功！";
        return true;
    } else {
        locker.relock();
        m_isVoiceCalling = false;
        m_voiceData.isVoiceCalling = false;
        qCritical() << "[VoiceManager] 启动失败：音频流启动失败";
        return false;
    }
}

// 2. 修改 stopVoiceCall 函数，增加停止原因日志
void VoiceManager::stopVoiceCall()
{
    QMutexLocker locker(&m_voiceMutex);
    if (!m_isVoiceCalling) {
        qDebug() << "[VoiceManager] 停止通话：当前未在通话中，无需操作";
        return;
    }

    qDebug() << "[VoiceManager] 主动停止语音通话，开始清理资源...";
    m_isVoiceCalling = false;
    m_voiceData.isVoiceCalling = false;
    locker.unlock();

    emit sigStopTimers(); // 停止定时器
    stopAudioStreams();
    emit voiceCallStopped();
    qDebug() << "[VoiceManager] 语音通话已停止";
}

bool VoiceManager::startAudioStreams()
{
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        emit errorOccurred("PortAudio初始化失败：" + QString(Pa_GetErrorText(err)));
        return false;
    }

    // ===== 1. 先初始化输入流，获取实际可用的采样率/设备参数 =====
    PaDeviceIndex inputDeviceId = Pa_GetDefaultInputDevice();
    if (inputDeviceId == paNoDevice) {
        emit errorOccurred("无可用麦克风设备");
        Pa_Terminate();
        return false;
    }
    const PaDeviceInfo* inputDevInfo = Pa_GetDeviceInfo(inputDeviceId);
    // 优先使用设备原生采样率（避免强制44100导致不兼容）
    m_currentSampleRate = inputDevInfo->defaultSampleRate;
    // 降级处理：若原生采样率异常，用通用的16000（语音场景更稳定）
    if (m_currentSampleRate <= 0 || m_currentSampleRate > 48000) {
        m_currentSampleRate = 16000;
    }
    // 缓冲区帧数：用设备推荐值（避免硬编码512导致不匹配）
    m_currentFrameSize = paFramesPerBufferUnspecified;
    if (inputDevInfo->defaultLowInputLatency > 0) {
        m_currentFrameSize = static_cast<unsigned long>(m_currentSampleRate * inputDevInfo->defaultLowInputLatency);
    }
    // 兜底：语音场景256帧更稳定（避免帧数过大/过小）
    if (m_currentFrameSize == 0 || m_currentFrameSize > 1024) {
        m_currentFrameSize = 256;
    }

    PaStreamParameters inputParams{};
    inputParams.device = inputDeviceId;
    inputParams.channelCount = 1;                // 严格单声道
    inputParams.sampleFormat = paInt16;          // 16位采样（小端序）
    inputParams.suggestedLatency = inputDevInfo->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;

    // ===== 2. 初始化输出流（严格对齐输入流参数）=====
    PaDeviceIndex outputDeviceId = Pa_GetDefaultOutputDevice();
    if (outputDeviceId == paNoDevice) {
        emit errorOccurred("无可用扬声器设备");
        Pa_Terminate();
        return false;
    }
    const PaDeviceInfo* outputDevInfo = Pa_GetDeviceInfo(outputDeviceId);
    qDebug() << "[VoiceManager] 音频参数：采样率=" << m_currentSampleRate
             << "缓冲区帧数=" << m_currentFrameSize
             << "输入设备=" << inputDevInfo->name
             << "输出设备=" << outputDevInfo->name;

    PaStreamParameters outputParams{};
    outputParams.device = outputDeviceId;
    outputParams.channelCount = 1;                // 和输入流严格一致
    outputParams.sampleFormat = paInt16;          // 和输入流严格一致
    // 输出延迟：用低延迟+和输入流同量级，避免不同步
    outputParams.suggestedLatency = qMin(outputDevInfo->defaultLowOutputLatency, inputDevInfo->defaultLowInputLatency);
    outputParams.hostApiSpecificStreamInfo = nullptr;

    // ===== 3. 打开输出流（参数和输入流完全对齐）=====
    err = Pa_OpenStream(
        &m_outputStream,
        nullptr,                // 无输入
        &outputParams,
        m_currentSampleRate,    // 动态采样率（不再硬编码）
        m_currentFrameSize,     // 动态缓冲区帧数（不再硬编码）
        paClipOff | paDitherOff, // 关闭抖动/裁剪，减少杂音
        nullptr,                // 输出流主动写入，无需回调
        nullptr
        );
    if (err != paNoError) {
        emit errorOccurred("打开输出流失败：" + QString(Pa_GetErrorText(err)));
        Pa_Terminate();
        return false;
    }

    // 启动输出流（增加启动前的状态检查）
    err = Pa_StartStream(m_outputStream);
    if (err != paNoError) {
        emit errorOccurred("启动输出流失败：" + QString(Pa_GetErrorText(err)));
        Pa_CloseStream(m_outputStream);
        Pa_Terminate();
        return false;
    }
    qDebug() << "[VoiceManager] 输出流启动成功，采样率=" << m_currentSampleRate << "帧大小=" << m_currentFrameSize;

    // ===== 4. 打开并启动输入流（对齐输出流参数）=====
    err = Pa_OpenStream(
        &m_inputStream,
        &inputParams,
        nullptr,
        m_currentSampleRate,    // 和输出流一致
        m_currentFrameSize,     // 和输出流一致
        paClipOff | paDitherOff,
        paInputCallback,
        &m_voiceData
        );
    if (err != paNoError) {
        emit errorOccurred("打开输入流失败：" + QString(Pa_GetErrorText(err)));
        Pa_CloseStream(m_outputStream);
        Pa_Terminate();
        return false;
    }

    err = Pa_StartStream(m_inputStream);
    if (err != paNoError) {
        emit errorOccurred("启动输入流失败：" + QString(Pa_GetErrorText(err)));
        Pa_CloseStream(m_inputStream);
        Pa_CloseStream(m_outputStream);
        Pa_Terminate();
        return false;
    }

    // 动态调整发送定时器间隔（匹配帧大小和采样率）
    if (m_sendAudioTimer) {
        // 计算每帧的时长（ms）= 帧大小 / 采样率 * 1000
        int interval = static_cast<int>((m_currentFrameSize / m_currentSampleRate) * 1000);
        // 兜底：间隔10-50ms之间
        interval = qBound(10, interval, 50);
        m_sendAudioTimer->setInterval(interval);
        qDebug() << "[VoiceManager] 发送定时器间隔调整为：" << interval << "ms";
    }

    return true;
}

bool VoiceManager::isVoiceCalling()
{
    QMutexLocker locker(&m_voiceMutex);
    return m_isVoiceCalling;
}

void VoiceManager::onUdpReadyRead()
{
    QMutexLocker locker(&m_voiceMutex);
    if (!m_isVoiceCalling || !m_outputStream || Pa_IsStreamActive(m_outputStream) != 1) {
        return;
    }

    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        qint64 readLen = m_udpSocket->readDatagram(datagram.data(), datagram.size());
        if (readLen <= 0) continue;
        qDebug()<<datagram;
        // 1. 拆分ID和音频数据（处理粘包：可能多个包粘在一起）
        int splitPos = datagram.indexOf('|');
        if (splitPos == -1) {
            qWarning() << "[接收端] 数据包格式错误，无分隔符";
            continue;
        }
        QByteArray voiceData = datagram.mid(splitPos + 1);
        if (voiceData.isEmpty()) continue;

        // 2. 拼接剩余数据（解决UDP拆包：单次数据不足一帧）
        m_remainingAudioData += voiceData;
        // 每帧是2字节（16位）*1声道 = 2字节/帧，计算可完整写入的帧数
        int frameByteSize = static_cast<int>(m_currentFrameSize * 2);
        if (m_remainingAudioData.size() < frameByteSize) {
            continue; // 数据不足一帧，缓存等待后续包
        }

        // 3. 按帧拆分数据（确保写入的是整数帧，避免格式错乱）
        while (m_remainingAudioData.size() >= frameByteSize) {
            QByteArray frameData = m_remainingAudioData.left(frameByteSize);
            m_remainingAudioData.remove(0, frameByteSize);

            // 4. 校验数据有效性（排除无效数据导致的呲声）
            const int16_t* samples = reinterpret_cast<const int16_t*>(frameData.constData());
            bool isValid = false;
            for (unsigned long i = 0; i < m_currentFrameSize; i++) {
                if (samples[i] != 0 && samples[i] != -1) {
                    isValid = true;
                    break;
                }
            }
            if (!isValid) continue;

            // 5. 写入输出流（关键：帧数是m_currentFrameSize，不是字节数/2）
            PaError err = Pa_WriteStream(m_outputStream, frameData.data(), m_currentFrameSize);
            if (err != paNoError) {
                qWarning() << "[接收端] 写入音频失败：" << Pa_GetErrorText(err) << "尝试重启流";
                // 流异常恢复（更优雅的重启逻辑）
                Pa_StopStream(m_outputStream);
                Pa_Sleep(10); // 短暂休眠避免频繁重启
                if (Pa_StartStream(m_outputStream) != paNoError) {
                    emit errorOccurred("输出流重启失败，语音播放中断");
                }
            }
        }
    }
}

void VoiceManager::sendCachedAudioData()
{
    QByteArray allData;
    {
        QMutexLocker locker(&m_queueMutex);
        if (m_audioDataQueue.isEmpty() || !m_isVoiceCalling) return;
        // 限制单次发送的数据量（UDP建议不超过1400字节，避免IP分片）
        const int maxUdpSize = 1400;
        while (!m_audioDataQueue.isEmpty() && (allData.size() + m_audioDataQueue.head().size()) <= maxUdpSize) {
            allData += m_audioDataQueue.dequeue();
        }
    }

    // 过滤全0/全FF的无效数据
    bool isAllInvalid = true;
    const int16_t* samples = reinterpret_cast<const int16_t*>(allData.constData());
    int sampleCount = allData.size() / 2;
    for (int i = 0; i < sampleCount; i++) {
        if (samples[i] != 0 && samples[i] != -1) {
            isAllInvalid = false;
            break;
        }
    }
    if (isAllInvalid || allData.isEmpty()) {
        qDebug() << "[发送端] 无有效音频数据，跳过UDP发送";
        return;
    }

    // 正常构造包并发送
    QByteArray udpPacket;
    {
        QMutexLocker voiceLocker(&m_voiceMutex);
        udpPacket = QString("%1:%2|").arg(m_senderId).arg(m_receiverId).toUtf8() + allData;
    }

    if (!m_udpSocket) return;
    qint64 len = m_udpSocket->writeDatagram(udpPacket, QHostAddress(m_udpServerIp), m_udpServerPort);
    if (len == -1) emit errorOccurred("UDP发送失败：" + m_udpSocket->errorString());
}

void VoiceManager::checkAudioStreamStatus()
{
    QMutexLocker locker(&m_voiceMutex);
    if (!m_isVoiceCalling) return;

    bool streamError = false;
    if (m_inputStream && Pa_IsStreamActive(m_inputStream) != 1) {
        emit errorOccurred("音频输入流已停止");
        streamError = true;
    }
    if (m_outputStream && Pa_IsStreamActive(m_outputStream) != 1) {
        emit errorOccurred("音频输出流已停止");
        streamError = true;
    }

    // 若流异常，尝试重启（可选）
    if (streamError) {
        stopAudioStreams();
        startAudioStreams();
    }
}

bool VoiceManager::initPortAudio()
{
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        qCritical() << "PortAudio初始化失败：" << Pa_GetErrorText(err);
        return false;
    }
    return true;
}

void VoiceManager::stopAudioStreams()
{
    QMutexLocker queueLocker(&m_queueMutex);
    m_audioDataQueue.clear();
    m_remainingAudioData.clear(); // 清空缓存的音频数据
    queueLocker.unlock();

    QMutexLocker locker(&m_voiceMutex);
    if (m_inputStream) {
        Pa_StopStream(m_inputStream);
        Pa_CloseStream(m_inputStream);
        m_inputStream = nullptr;
    }
    if (m_outputStream) {
        Pa_StopStream(m_outputStream);
        Pa_CloseStream(m_outputStream);
        m_outputStream = nullptr;
    }
}

int VoiceManager::paInputCallback(const void* inputBuffer, void* outputBuffer,
                                  unsigned long framesPerBuffer,
                                  const PaStreamCallbackTimeInfo* timeInfo,
                                  PaStreamCallbackFlags statusFlags,
                                  void* userData)
{
    (void)outputBuffer; (void)timeInfo; (void)statusFlags;
    VoiceData* data = reinterpret_cast<VoiceData*>(userData);
    if (!data) return paContinue;

    // 1. 线程安全读取通话状态
    bool isCalling = false;
    if (data->mutex) {
        QMutexLocker locker(data->mutex);
        isCalling = data->isVoiceCalling;
    }

    // 2. 处理采集数据
    if (isCalling && inputBuffer) {
        const int16_t* audioSamples = static_cast<const int16_t*>(inputBuffer);

        // 3. 过滤全0/全FF的无效数据（避免发送空数据）
        bool isAllInvalid = true;
        for (unsigned long i = 0; i < framesPerBuffer; i++) {
            int16_t sample = audioSamples[i];
            if (sample != 0 && sample != -1) { // 排除0和-1（0xFFFF）
                isAllInvalid = false;
                break;
            }
        }
        if (isAllInvalid) {
            return paContinue;
        }

        // 4. 正常拷贝数据到队列
        QByteArray audioData(static_cast<const char*>(inputBuffer), framesPerBuffer * 2);
        VoiceManager* manager = data->managerInstance;
        if (manager) {
            QMutexLocker locker(&manager->m_queueMutex);
            manager->m_audioDataQueue.enqueue(audioData);
        }
    }

    return paContinue;
}

int VoiceManager::paOutputCallback(const void* inputBuffer, void* outputBuffer,
                                   unsigned long framesPerBuffer,
                                   const PaStreamCallbackTimeInfo* timeInfo,
                                   PaStreamCallbackFlags statusFlags,
                                   void* userData)
{
    (void)inputBuffer; (void)timeInfo; (void)statusFlags; (void)userData;
    memset(outputBuffer, 0, framesPerBuffer * 2);
    return paContinue;
}
