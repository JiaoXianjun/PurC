/*
 * @file fetcher-process.cpp
 * @author XueShuming
 * @date 2021/11/17
 * @brief The impl for fetcher process.
 *
 * Copyright (C) 2021 FMSoft <https://www.fmsoft.cn>
 *
 * This file is a part of PurC (short for Purring Cat), an HVML interpreter.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "config.h"

#if ENABLE(REMOTE_FETCHER)

#include "fetcher-process.h"
#include "fetcher-messages.h"
#include "purc-rwstream.h"

#include "NetworkProcessCreationParameters.h"

#include <wtf/RunLoop.h>

using namespace PurCFetcher;

struct process_async_data {
    PcFetcherProcess *process;
    PcFetcherSession *session;
    pcfetcher_response_handler handler;
    void *ctxt;
};

PcFetcherProcess::PcFetcherProcess(struct pcfetcher* fetcher,
        bool alwaysRunsAtBackgroundPriority)
 : m_fetcher(fetcher)
 , m_alwaysRunsAtBackgroundPriority(alwaysRunsAtBackgroundPriority)
{
}

PcFetcherProcess::~PcFetcherProcess()
{
    reset();
}

void PcFetcherProcess::reset(void)
{
    if (m_connection) {
        m_connection->invalidate();
        m_connection = nullptr;
    }

    if (m_processLauncher) {
        m_processLauncher->invalidate();
        m_processLauncher = nullptr;
    }
    auto pendingMessages = WTFMove(m_pendingMessages);
    for (auto& pendingMessage : pendingMessages) {
        if (pendingMessage.asyncReplyInfo)
            pendingMessage.asyncReplyInfo->first(nullptr);
    }

    size_t sz = this->m_asyncSessionWrap.size();
    if (sz) {
        Vector<void*> tmpVec = m_asyncSessionWrap;
        for (size_t i = 0; i < sz; i++) {
            struct process_async_data *data =
                (struct process_async_data *)tmpVec[i];
            data->session->stop();
        }
    }
}

void PcFetcherProcess::getLaunchOptions(
        ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processIdentifier = m_processIdentifier;

    if (const char* userDirectorySuffix = getenv("DIRHELPER_USER_DIR_SUFFIX"))
        launchOptions.extraInitializationData.add("user-directory-suffix"_s, userDirectorySuffix);

    if (m_alwaysRunsAtBackgroundPriority)
        launchOptions.extraInitializationData.add("always-runs-at-background-priority"_s, "true");

    launchOptions.processType = ProcessLauncher::ProcessType::Fetcher;
}

void PcFetcherProcess::connect()
{
    ASSERT(!m_processLauncher);
    ProcessLauncher::LaunchOptions launchOptions;
    getLaunchOptions(launchOptions);
    m_processLauncher = ProcessLauncher::create(this, WTFMove(launchOptions));
    initFetcherProcess();
}

void PcFetcherProcess::terminate()
{
    if (m_processLauncher)
        m_processLauncher->terminateProcess();
}

void PcFetcherProcess::initFetcherProcess()
{
    NetworkProcessCreationParameters parameters;
    send(Messages::NetworkProcess::InitializeNetworkProcess(parameters), 0);
}

PcFetcherProcess::State PcFetcherProcess::state() const
{
    if (m_processLauncher && m_processLauncher->isLaunching())
        return PcFetcherProcess::State::Launching;

    if (!m_connection)
        return PcFetcherProcess::State::Terminated;

    return PcFetcherProcess::State::Running;
}

bool PcFetcherProcess::wasTerminated() const
{
    switch (state()) {
    case PcFetcherProcess::State::Launching:
        return false;
    case PcFetcherProcess::State::Terminated:
        return true;
    case PcFetcherProcess::State::Running:
        break;
    }

    auto pid = processIdentifier();
    if (!pid)
        return true;

    return false;
}

bool PcFetcherProcess::sendMessage(std::unique_ptr<IPC::Encoder> encoder,
        OptionSet<IPC::SendOption> sendOptions,
        Optional<std::pair<CompletionHandler<void(IPC::Decoder*)>, uint64_t>>&& asyncReplyInfo,
        ShouldStartProcessThrottlerActivity shouldStartProcessThrottlerActivity)
{
    if (asyncReplyInfo && canSendMessage() &&
            shouldStartProcessThrottlerActivity == ShouldStartProcessThrottlerActivity::Yes) {
        auto completionHandler = std::exchange(asyncReplyInfo->first, nullptr);
        asyncReplyInfo->first = [completionHandler = WTFMove(completionHandler)](IPC::Decoder* decoder) mutable {
            completionHandler(decoder);
        };
    }

    switch (state()) {
    case State::Launching:
        // If we're waiting for the child process to launch, we need to stash away the messages so we can send them once we have a connection.
        m_pendingMessages.append(
                { WTFMove(encoder), sendOptions, WTFMove(asyncReplyInfo) }
                );
        return true;

    case State::Running:
        if (asyncReplyInfo)
            IPC::addAsyncReplyHandler(*connection(), asyncReplyInfo->second,
                    std::exchange(asyncReplyInfo->first, nullptr));
        if (connection()->sendMessage(WTFMove(encoder), sendOptions))
            return true;
        break;

    case State::Terminated:
        break;
    }

    if (asyncReplyInfo && asyncReplyInfo->first) {
        RunLoop::current().dispatch(
                [completionHandler = WTFMove(asyncReplyInfo->first)]() mutable {
            completionHandler(nullptr);
        });
    }

    return false;
}

void PcFetcherProcess::didReceiveMessage(IPC::Connection& connection,
        IPC::Decoder& decoder)
{
    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
}

void PcFetcherProcess::didReceiveSyncMessage(IPC::Connection& connection,
        IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
    UNUSED_PARAM(replyEncoder);
}

void PcFetcherProcess::didFinishLaunching(ProcessLauncher*,
        IPC::Connection::Identifier connectionIdentifier)
{
    ASSERT(!m_connection);

    if (!IPC::Connection::identifierIsValid(connectionIdentifier))
        return;

    m_connection = IPC::Connection::createServerConnection(
            connectionIdentifier, *this);

    m_connection->open();

    for (auto&& pendingMessage : std::exchange(m_pendingMessages, { })) {
        if (!shouldSendPendingMessage(pendingMessage))
            continue;
        auto encoder = WTFMove(pendingMessage.encoder);
        auto sendOptions = pendingMessage.sendOptions;
        if (pendingMessage.asyncReplyInfo)
            IPC::addAsyncReplyHandler(*connection(),
                    pendingMessage.asyncReplyInfo->second,
                    WTFMove(pendingMessage.asyncReplyInfo->first));
        m_connection->sendMessage(WTFMove(encoder), sendOptions);
    }
}

void PcFetcherProcess::shutDownProcess()
{
    switch (state()) {
    case State::Launching:
        m_processLauncher->invalidate();
        m_processLauncher = nullptr;
        break;
    case State::Running:
        break;
    case State::Terminated:
        return;
    }

    if (!m_connection)
        return;

#if 0
    if (canSendMessage())
        send(Messages::AuxiliaryProcess::ShutDown(), 0);
#endif

    m_connection->invalidate();
    m_connection = nullptr;
}

void PcFetcherProcess::setProcessSuppressionEnabled(bool processSuppressionEnabled)
{
    UNUSED_PARAM(processSuppressionEnabled);
}

PcFetcherSession* PcFetcherProcess::createSession(void)
{
    PurCFetcher::ProcessIdentifier pid = ProcessIdentifier::generate();
    PAL::SessionID sid(1);
    Optional<IPC::Attachment> attachment;
    PurCFetcher::HTTPCookieAcceptPolicy cookieAcceptPolicy;
    sendSync(
        Messages::NetworkProcess::CreateNetworkConnectionToWebProcess { pid, sid },
        Messages::NetworkProcess::CreateNetworkConnectionToWebProcess::Reply(
            attachment, cookieAcceptPolicy), 0);
    return new PcFetcherSession(sid.toUInt64(),
            attachment->releaseFileDescriptor());
}

purc_variant_t PcFetcherProcess::requestAsync(
        const char* base_uri,
        const char* url,
        enum pcfetcher_request_method method,
        purc_variant_t params,
        uint32_t timeout,
        pcfetcher_response_handler handler,
        void* ctxt)
{
    PcFetcherSession* session = createSession();
    if (!session) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return PURC_VARIANT_INVALID;
    }

    struct process_async_data *data = new struct process_async_data;
    if (!data) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        delete session;
        return PURC_VARIANT_INVALID;
    }
    data->process = this;
    data->session = session;
    data->handler = handler;
    data->ctxt = ctxt;
    purc_variant_t ret = session->requestAsync(base_uri, url, method,
            params, timeout, asyncRespHandler, data);
    m_asyncSessionWrap.append(data);
    return ret;
}

void PcFetcherProcess::asyncRespHandler(purc_variant_t request_id, void *ctxt,
        const struct pcfetcher_resp_header *resp_header,
        purc_rwstream_t resp)
{
    struct process_async_data *data = (struct process_async_data*)ctxt;
    data->process->m_asyncSessionWrap.removeFirst(data);
    data->handler(request_id, data->ctxt, resp_header, resp);
    delete data;
}

purc_rwstream_t PcFetcherProcess::requestSync(
        const char* base_uri,
        const char* url,
        enum pcfetcher_request_method method,
        purc_variant_t params,
        uint32_t timeout,
        struct pcfetcher_resp_header *resp_header)
{
    PcFetcherSession* session = createSession();
    purc_rwstream_t rws = session->requestSync(base_uri, url, method,
            params, timeout, resp_header);
    delete session;
    return rws;
}

void PcFetcherProcess::cancelAsyncRequest(purc_variant_t request_id)
{
    size_t sz = m_asyncSessionWrap.size();
    for (size_t i = 0; i < sz; i++) {
        struct process_async_data *data =
            (struct process_async_data *)m_asyncSessionWrap[i];
        if (data->session->getRequestId() == request_id) {
            data->session->cancel();
            break;
        }
    }
}

int PcFetcherProcess::checkResponse(uint32_t timeout_ms)
{
    UNUSED_PARAM(timeout_ms);
    return 0;
}

void PcFetcherProcess::didClose(IPC::Connection&)
{
    reset();
    RunLoop::main().dispatch([process=this] {
        process->connect();
    });
}

void PcFetcherProcess::didReceiveInvalidMessage(IPC::Connection&,
        IPC::MessageName)
{
}

#endif // ENABLE(REMOTE_FETCHER)

