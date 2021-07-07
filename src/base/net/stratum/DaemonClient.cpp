/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2019      Howard Chu  <https://github.com/hyc>
 * Copyright 2018-2020 SChernykh   <https://github.com/SChernykh>
 * Copyright 2016-2020 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include "base/net/stratum/DaemonClient.h"
#include "3rdparty/http-parser/http_parser.h"
#include "base/io/json/Json.h"
#include "base/io/json/JsonRequest.h"
#include "base/io/log/Log.h"
#include "base/kernel/interfaces/IClientListener.h"
#include "base/net/http/HttpClient.h"
#include "base/net/stratum/SubmitResult.h"
#include "base/tools/Buffer.h"
#include "base/tools/Cvt.h"
#include "base/tools/Timer.h"
#include "base/tools/cryptonote/Signatures.h"
#include "base/tools/cryptonote/WalletAddress.h"
#include "net/JobResult.h"
#include "3rdparty/rapidjson/document.h"
#include "3rdparty/rapidjson/error/en.h"
#include "3rdparty/rapidjson/stringbuffer.h"
#include "3rdparty/rapidjson/writer.h"


#ifdef XMRIG_FEATURE_TLS
#   include "base/net/http/HttpsClient.h"
#endif


#include <algorithm>
#include <cassert>


namespace xmrig {

static const char *kBlocktemplateBlob       = "blocktemplate_blob";
static const char *kGetHeight               = "/getheight";
static const char *kGetInfo                 = "/getinfo";
static const char *kHash                    = "hash";
static const char *kHeight                  = "height";
static const char *kJsonRPC                 = "/json_rpc";

static const size_t kBlobReserveSize        = 8;

}


xmrig::DaemonClient::DaemonClient(int id, IClientListener *listener) :
    BaseClient(id, listener),
    m_apiVersion(API_MONERO)
{
    m_httpListener  = std::make_shared<HttpListener>(this);
    m_timer         = new Timer(this);
}


xmrig::DaemonClient::~DaemonClient()
{
    delete m_timer;
}


bool xmrig::DaemonClient::disconnect()
{
    if (m_state != UnconnectedState) {
        setState(UnconnectedState);
    }

    return true;
}


bool xmrig::DaemonClient::isTLS() const
{
#   ifdef XMRIG_FEATURE_TLS
    return m_pool.isTLS();
#   else
    return false;
#   endif
}


int64_t xmrig::DaemonClient::submit(const JobResult &result)
{
    if (result.jobId != (m_blocktemplateStr.data() + m_blocktemplateStr.size() - 32)) {
        return -1;
    }

    char *data = (m_apiVersion == API_DERO) ? m_blockhashingblob.data() : m_blocktemplateStr.data();

    const size_t sig_offset = m_job.nonceOffset() + m_job.nonceSize();

#   ifdef XMRIG_PROXY_PROJECT

    memcpy(data + m_job.nonceOffset() * 2, result.nonce, 8);

    if (m_blocktemplate.has_miner_signature && result.sig) {
        memcpy(data + sig_offset * 2, result.sig, 64 * 2);
        memcpy(data + m_blocktemplate.tx_pubkey_index * 2, result.sig_data, 32 * 2);
        memcpy(data + m_blocktemplate.eph_public_key_index * 2, result.sig_data + 32 * 2, 32 * 2);
    }

#   else

    Cvt::toHex(data + m_job.nonceOffset() * 2, 8, reinterpret_cast<const uint8_t*>(&result.nonce), 4);

    if (m_blocktemplate.has_miner_signature) {
        Cvt::toHex(data + sig_offset * 2, 128, result.minerSignature(), 64);
    }

#   endif

    using namespace rapidjson;
    Document doc(kObjectType);

    Value params(kArrayType);
    if (m_apiVersion == API_DERO) {
        params.PushBack(m_blocktemplateStr.toJSON(), doc.GetAllocator());
        params.PushBack(m_blockhashingblob.toJSON(), doc.GetAllocator());
    }
    else {
        params.PushBack(m_blocktemplateStr.toJSON(), doc.GetAllocator());
    }

    JsonRequest::create(doc, m_sequence, "submitblock", params);

#   ifdef XMRIG_PROXY_PROJECT
    m_results[m_sequence] = SubmitResult(m_sequence, result.diff, result.actualDiff(), result.id, 0);
#   else
    m_results[m_sequence] = SubmitResult(m_sequence, result.diff, result.actualDiff(), 0, result.backend);
#   endif

    send(HTTP_POST, kJsonRPC, doc);

    return m_sequence++;
}


void xmrig::DaemonClient::connect()
{
    if ((m_pool.algorithm() == Algorithm::ASTROBWT_DERO) || (m_pool.coin() == Coin::DERO)) {
        m_apiVersion = API_DERO;
    }

    setState(ConnectingState);
    getBlockTemplate();
}


void xmrig::DaemonClient::connect(const Pool &pool)
{
    setPool(pool);
    connect();
}


void xmrig::DaemonClient::onHttpData(const HttpData &data)
{
    if (data.status != HTTP_STATUS_OK) {
        return retry();
    }

    LOG_DEBUG("[%s:%d] received (%d bytes): \"%.*s\"", m_pool.host().data(), m_pool.port(), static_cast<int>(data.body.size()), static_cast<int>(data.body.size()), data.body.c_str());

    m_ip = static_cast<const HttpContext &>(data).ip().c_str();

#   ifdef XMRIG_FEATURE_TLS
    if (isTLS()) {
        m_tlsVersion     = static_cast<const HttpsClient &>(data).version();
        m_tlsFingerprint = static_cast<const HttpsClient &>(data).fingerprint();
    }
#   endif

    rapidjson::Document doc;
    if (doc.Parse(data.body.c_str()).HasParseError()) {
        if (!isQuiet()) {
            LOG_ERR("[%s:%d] JSON decode failed: \"%s\"", m_pool.host().data(), m_pool.port(), rapidjson::GetParseError_En(doc.GetParseError()));
        }

        return retry();
    }

    if (data.method == HTTP_GET) {
        if (data.url == kGetHeight) {
            if (!doc.HasMember(kHash)) {
                m_apiVersion = API_CRYPTONOTE_DEFAULT;

                return send(HTTP_GET, kGetInfo);
            }

            if (isOutdated(Json::getUint64(doc, kHeight), Json::getString(doc, kHash))) {
                getBlockTemplate();
            }
        }
        else if (data.url == kGetInfo && isOutdated(Json::getUint64(doc, kHeight), Json::getString(doc, "top_block_hash"))) {
            getBlockTemplate();
        }

        return;
    }

    if (!parseResponse(Json::getInt64(doc, "id", -1), Json::getObject(doc, "result"), Json::getObject(doc, "error"))) {
        retry();
    }
}


void xmrig::DaemonClient::onTimer(const Timer *)
{
    if (m_state == ConnectingState) {
        getBlockTemplate();
    }
    else if (m_state == ConnectedState) {
        if (m_apiVersion == API_DERO) {
            using namespace rapidjson;
            Document doc(kObjectType);
            auto& allocator = doc.GetAllocator();

            doc.AddMember("id", m_sequence, allocator);
            doc.AddMember("jsonrpc", "2.0", allocator);
            doc.AddMember("method", "get_info", allocator);

            send(HTTP_POST, kJsonRPC, doc);
            ++m_sequence;
        }
        else {
            send(HTTP_GET, (m_apiVersion == API_MONERO) ? kGetHeight : kGetInfo);
        }
    }
}


bool xmrig::DaemonClient::isOutdated(uint64_t height, const char *hash) const
{
    return m_job.height() != height || m_prevHash != hash;
}


bool xmrig::DaemonClient::parseJob(const rapidjson::Value &params, int *code)
{
    Job job(false, m_pool.algorithm(), String());

    String blocktemplate = Json::getString(params, kBlocktemplateBlob);

    if (blocktemplate.isNull()) {
        LOG_ERR("Empty block template received from daemon");
        *code = 1;
        return false;
    }

    Coin pool_coin = m_pool.coin();

    if (!pool_coin.isValid() && (m_pool.algorithm() == Algorithm::RX_WOW)) {
        pool_coin = Coin::WOWNERO;
    }

    if (!m_blocktemplate.Init(blocktemplate, pool_coin)) {
        LOG_ERR("Invalid block template received from daemon");
        *code = 2;
        return false;
    }

    m_blockhashingblob = Json::getString(params, "blockhashing_blob");

    if (m_blocktemplate.has_miner_signature) {
        if (m_pool.spendSecretKey().isEmpty()) {
            LOG_ERR("Secret spend key is not set");
            *code = 4;
            return false;
        }

        if (m_pool.spendSecretKey().size() != 64) {
            LOG_ERR("Secret spend key has invalid length. It must be 64 hex characters.");
            *code = 5;
            return false;
        }

        uint8_t secret_spendkey[32];
        if (!Cvt::fromHex(secret_spendkey, 32, m_pool.spendSecretKey(), 64)) {
            LOG_ERR("Secret spend key is not a valid hex data.");
            *code = 6;
            return false;
        }

        uint8_t public_spendkey[32];
        if (!secret_key_to_public_key(secret_spendkey, public_spendkey)) {
            LOG_ERR("Secret spend key is invalid.");
            *code = 7;
            return false;
        }

#       ifdef XMRIG_PROXY_PROJECT
        job.setSpendSecretKey(secret_spendkey);
        job.setMinerTx(
            m_blocktemplate.raw_blob.data() + m_blocktemplate.miner_tx_prefix_begin_index,
            m_blocktemplate.raw_blob.data() + m_blocktemplate.miner_tx_prefix_end_index,
            m_blocktemplate.eph_public_key_index - m_blocktemplate.miner_tx_prefix_begin_index,
            m_blocktemplate.tx_pubkey_index - m_blocktemplate.miner_tx_prefix_begin_index,
            m_blocktemplate.miner_tx_merkle_tree_branch
        );
#       else
        uint8_t secret_viewkey[32];
        derive_view_secret_key(secret_spendkey, secret_viewkey);

        uint8_t public_viewkey[32];
        if (!secret_key_to_public_key(secret_viewkey, public_viewkey)) {
            LOG_ERR("Secret view key is invalid.");
            *code = 8;
            return false;
        }

        uint8_t derivation[32];
        if (!generate_key_derivation(m_blocktemplate.raw_blob.data() + m_blocktemplate.tx_pubkey_index, secret_viewkey, derivation)) {
            LOG_ERR("Failed to generate key derivation for miner signature.");
            *code = 9;
            return false;
        }

        WalletAddress user_address;
        if (!user_address.Decode(m_pool.user())) {
            LOG_ERR("Invalid wallet address.");
            *code = 10;
            return false;
        }

        if (memcmp(user_address.public_spend_key, public_spendkey, sizeof(public_spendkey)) != 0) {
            LOG_ERR("Wallet address and spend key don't match.");
            *code = 11;
            return false;
        }

        if (memcmp(user_address.public_view_key, public_viewkey, sizeof(public_viewkey)) != 0) {
            LOG_ERR("Wallet address and view key don't match.");
            *code = 12;
            return false;
        }

        uint8_t eph_secret_key[32];
        derive_secret_key(derivation, 0, secret_spendkey, eph_secret_key);

        job.setEphemeralKeys(m_blocktemplate.raw_blob.data() + m_blocktemplate.eph_public_key_index, eph_secret_key);
#       endif
    }

    if (m_apiVersion == API_DERO) {
        const uint64_t offset = Json::getUint64(params, "reserved_offset");
        Cvt::toHex(m_blockhashingblob.data() + offset * 2, kBlobReserveSize * 2, Cvt::randomBytes(kBlobReserveSize).data(), kBlobReserveSize);
    }

    if (pool_coin.isValid()) {
        job.setAlgorithm(pool_coin.algorithm(m_blocktemplate.major_version));
    }

    if (!job.setBlob(m_blockhashingblob)) {
        *code = 3;
        return false;
    }

    job.setSeedHash(Json::getString(params, "seed_hash"));
    job.setHeight(Json::getUint64(params, kHeight));
    job.setDiff(Json::getUint64(params, "difficulty"));

    job.setId(blocktemplate.data() + blocktemplate.size() - 32);

    if (m_pool.coin().isValid()) {
        job.setAlgorithm(m_pool.coin().algorithm(job.blob()[0]));
    }

    m_job              = std::move(job);
    m_blocktemplateStr = std::move(blocktemplate);
    m_prevHash         = Json::getString(params, "prev_hash");

    if (m_apiVersion == API_DERO) {
        // Truncate to 32 bytes to have the same data as in get_info RPC
        if (m_prevHash.size() > 64) {
            m_prevHash.data()[64] = '\0';
        }
    }

    if (m_state == ConnectingState) {
        setState(ConnectedState);
    }

    m_listener->onJobReceived(this, m_job, params);
    return true;
}


bool xmrig::DaemonClient::parseResponse(int64_t id, const rapidjson::Value &result, const rapidjson::Value &error)
{
    if (id == -1) {
        return false;
    }

    if (error.IsObject()) {
        const char *message = error["message"].GetString();

        if (!handleSubmitResponse(id, message) && !isQuiet()) {
            LOG_ERR("[%s:%d] error: " RED_BOLD("\"%s\"") RED_S ", code: %d", m_pool.host().data(), m_pool.port(), message, error["code"].GetInt());
        }

        return false;
    }

    if (!result.IsObject()) {
        return false;
    }

    if (result.HasMember("top_block_hash")) {
        if (m_prevHash != Json::getString(result, "top_block_hash")) {
            getBlockTemplate();
        }
        return true;
    }

    int code = -1;
    if (result.HasMember(kBlocktemplateBlob) && parseJob(result, &code)) {
        return true;
    }

    const char* error_msg = nullptr;

    if ((m_apiVersion == API_DERO) && result.HasMember("status")) {
        error_msg = result["status"].GetString();
        if (!error_msg || (strlen(error_msg) == 0) || (strcmp(error_msg, "OK") == 0)) {
            error_msg = nullptr;
        }
    }

    if (handleSubmitResponse(id, error_msg)) {
        getBlockTemplate();
        return true;
    }


    return false;
}


int64_t xmrig::DaemonClient::getBlockTemplate()
{
    using namespace rapidjson;
    Document doc(kObjectType);
    auto &allocator = doc.GetAllocator();

    Value params(kObjectType);
    params.AddMember("wallet_address", m_user.toJSON(), allocator);
    if (m_apiVersion == API_DERO) {
        params.AddMember("reserve_size", static_cast<uint64_t>(kBlobReserveSize), allocator);
    }
    else {
        params.AddMember("extra_nonce", Cvt::toHex(Cvt::randomBytes(kBlobReserveSize)).toJSON(doc), allocator);
    }

    JsonRequest::create(doc, m_sequence, "getblocktemplate", params);

    send(HTTP_POST, kJsonRPC, doc);

    return m_sequence++;
}


void xmrig::DaemonClient::retry()
{
    m_failures++;
    m_listener->onClose(this, static_cast<int>(m_failures));

    if (m_failures == -1) {
        return;
    }

    if (m_state == ConnectedState) {
        setState(ConnectingState);
    }

    m_timer->stop();
    m_timer->start(m_retryPause, 0);
}


void xmrig::DaemonClient::send(int method, const char *url, const char *data, size_t size)
{
    LOG_DEBUG("[%s:%d] " MAGENTA_BOLD("\"%s %s\"") BLACK_BOLD_S " send (%zu bytes): \"%.*s\"",
              m_pool.host().data(),
              m_pool.port(),
              http_method_str(static_cast<http_method>(method)),
              url,
              size,
              static_cast<int>(size),
              data);

    HttpClient *client;
#   ifdef XMRIG_FEATURE_TLS
    if (m_pool.isTLS()) {
        client = new HttpsClient(method, url, m_httpListener, data, size, m_pool.fingerprint());
    }
    else
#   endif
    {
        client = new HttpClient(method, url, m_httpListener, data, size);
    }

    client->setQuiet(isQuiet());
    client->connect(m_pool.host(), m_pool.port());

    if (method != HTTP_GET) {
        client->headers.insert({ "Content-Type", "application/json" });
    }
}


void xmrig::DaemonClient::send(int method, const char *url, const rapidjson::Document &doc)
{
    using namespace rapidjson;

    StringBuffer buffer(nullptr, 512);
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);

    send(method, url, buffer.GetString(), buffer.GetSize());
}


void xmrig::DaemonClient::setState(SocketState state)
{
    assert(m_state != state);
    if (m_state == state) {
        return;
    }

    m_state = state;

    switch (state) {
    case ConnectedState:
        {
            m_failures = 0;
            m_listener->onLoginSuccess(this);

            const uint64_t interval = std::max<uint64_t>(20, m_pool.pollInterval());
            m_timer->start(interval, interval);
        }
        break;

    case UnconnectedState:
        m_failures = -1;
        m_timer->stop();
        break;

    default:
        break;
    }
}
