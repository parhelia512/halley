#include "entity/entity_network_session.h"

#include <cassert>

#include "halley/bytes/compression.h"
#include "halley/entity/entity_factory.h"
#include "halley/entity/system.h"
#include "halley/entity/world.h"
#include "halley/support/logger.h"
#include "halley/utils/algorithm.h"

class NetworkComponent;
using namespace Halley;

EntityNetworkSession::EntityNetworkSession(std::shared_ptr<NetworkSession> session, Resources& resources, std::set<String> ignoreComponents, IEntityNetworkSessionListener* listener)
	: resources(resources)
	, listener(listener)
	, session(std::move(session))
{
	Expects(this->session);
	Expects(this->listener);
	
	this->session->addListener(this);
	this->session->setSharedDataHandler(this);

	entitySerializationOptions.type = EntitySerialization::Type::SaveData;

	deltaOptions.preserveOrder = false;
	deltaOptions.shallow = false;
	deltaOptions.deltaComponents = true;
	deltaOptions.ignoreComponents = std::move(ignoreComponents);

	setupDictionary();
	byteSerializationOptions.version = SerializerOptions::maxVersion;
	byteSerializationOptions.dictionary = &serializationDictionary;
}

EntityNetworkSession::~EntityNetworkSession()
{
	session->removeListener(this);
}

void EntityNetworkSession::setWorld(World& world, SystemMessageBridge bridge)
{
	factory = std::make_shared<EntityFactory>(world, resources);
	messageBridge = bridge;

	// Clear queue
	if (!queuedPackets.empty()) {
		for (auto& qp: queuedPackets) {
			processMessage(qp.fromPeerId, std::move(qp.message));
		}
		queuedPackets.clear();
	}
}

void EntityNetworkSession::sendUpdates(Time t, Rect4i viewRect, gsl::span<const std::pair<EntityId, uint8_t>> entityIds)
{
	// Update viewport
	auto& data = session->getMySharedData<EntityClientSharedData>();
	const bool first = !data.viewRect;
	data.viewRect = viewRect;
	if (first || data.getTimeSinceLastSend() > 0.05) {
		data.markModified();
	}

	// Update entities
	for (auto& peer: peers) {
		peer.sendEntities(t, entityIds, session->getClientSharedData<EntityClientSharedData>(peer.getPeerId()));
	}

	sendMessages();
	session->update(t);
}

void EntityNetworkSession::sendMessage(EntityNetworkMessage msg, NetworkSession::PeerId peerId)
{
	outbox[peerId].push_back(std::move(msg));
}

void EntityNetworkSession::sendMessages()
{
	for (const auto& [peerId, msgs]: outbox) {
		auto data = Serializer::toBytes(msgs, byteSerializationOptions);
		auto compressed = Compression::compressRaw(gsl::as_bytes(gsl::span<const Byte>(data)), false);
		auto packet = OutboundNetworkPacket(std::move(compressed));
		session->sendToPeer(std::move(packet), peerId);
	}
	outbox.clear();
}

void EntityNetworkSession::receiveUpdates()
{
	session->update(0.0);

	while (auto result = session->receive()) {
		const auto fromPeerId = result->first;
		auto& packet = result->second;

		auto bytes = Compression::decompressRaw(packet.getBytes(), 256 * 1024);
		auto msgs = Deserializer::fromBytes<Vector<EntityNetworkMessage>>(bytes, byteSerializationOptions);

		for (auto& msg: msgs) {
			if (canProcessMessage(msg)) {
				processMessage(fromPeerId, std::move(msg));
			} else {
				queuedPackets.emplace_back(QueuedMessage{ fromPeerId, std::move(msg) });
			}
		}
	}
}

bool EntityNetworkSession::canProcessMessage(const EntityNetworkMessage& msg) const
{
	return factory || !msg.needsInitialization();
}

void EntityNetworkSession::processMessage(NetworkSession::PeerId fromPeerId, EntityNetworkMessage msg)
{
	switch (msg.getType()) {
	case EntityNetworkHeaderType::Create:
	case EntityNetworkHeaderType::Destroy:
	case EntityNetworkHeaderType::Update:
		onReceiveEntityUpdate(fromPeerId, std::move(msg));
		break;
	case EntityNetworkHeaderType::ReadyToStart:
		onReceiveReady(fromPeerId, msg.getMessage<EntityNetworkMessageReadyToStart>());
		break;
	case EntityNetworkHeaderType::EntityMsg:
		onReceiveMessageToEntity(fromPeerId, msg.getMessage<EntityNetworkMessageEntityMsg>());
		break;
	case EntityNetworkHeaderType::SystemMsg:
		onReceiveSystemMessage(fromPeerId, msg.getMessage<EntityNetworkMessageSystemMsg>());
		break;
	case EntityNetworkHeaderType::SystemMsgResponse:
		onReceiveSystemMessageResponse(fromPeerId, msg.getMessage<EntityNetworkMessageSystemMsgResponse>());
		break;
	}
}

void EntityNetworkSession::onReceiveEntityUpdate(NetworkSession::PeerId fromPeerId, EntityNetworkMessage msg)
{
	for (auto& peer: peers) {
		if (peer.getPeerId() == fromPeerId) {
			peer.receiveNetworkMessage(fromPeerId, std::move(msg));
			return;
		}
	}
}

void EntityNetworkSession::onReceiveReady(NetworkSession::PeerId fromPeerId, const EntityNetworkMessageReadyToStart& msg)
{
	Logger::logDev("onReceiveReady from " + toString(int(fromPeerId)));
	if (fromPeerId == 0) {
		readyToStart = true;
	}
}

void EntityNetworkSession::onReceiveMessageToEntity(NetworkSession::PeerId fromPeerId, const EntityNetworkMessageEntityMsg& msg)
{
	Expects(factory);
	Expects(messageBridge.isValid());

	auto& world = factory->getWorld();

	const auto entity = world.findEntity(msg.entityUUID);
	if (entity) {
		messageBridge.sendMessageToEntity(entity->getEntityId(), msg.messageType, gsl::as_bytes(gsl::span<const Byte>(msg.messageData)));
	} else {
		Logger::logError("Received message for entity " + toString(msg.entityUUID) + ", but entity was not found.");
	}
}

void EntityNetworkSession::onReceiveSystemMessage(NetworkSession::PeerId fromPeerId, const EntityNetworkMessageSystemMsg& msg)
{
	Expects(factory);
	Expects(messageBridge.isValid());
	
	messageBridge.sendMessageToSystem(msg.targetSystem, msg.messageType, gsl::as_bytes(gsl::span<const Byte>(msg.messageData)));
}

void EntityNetworkSession::onReceiveSystemMessageResponse(NetworkSession::PeerId fromPeerId, const EntityNetworkMessageSystemMsgResponse& msg)
{
	auto iter = pendingSysMsgResponses.find(msg.msgId);
	if (iter == pendingSysMsgResponses.end()) {
		Logger::logError("Unexpected system message response received.");
		return;
	}

	iter->second.callback(const_cast<gsl::byte*>(reinterpret_cast<const gsl::byte*>(msg.responseData.data()))); // bad bad bad
	pendingSysMsgResponses.erase(iter);
}

void EntityNetworkSession::sendEntityMessage(EntityRef entity, int messageType, Bytes messageData)
{
	const NetworkSession::PeerId toPeerId = entity.getOwnerPeerId().value();
	sendMessage(EntityNetworkMessageEntityMsg(entity.getInstanceUUID(), messageType, std::move(messageData)), toPeerId);
}

void EntityNetworkSession::sendSystemMessage(String targetSystem, int messageType, Bytes messageData, SystemMessageDestination destination, std::function<void(gsl::byte*)> callback)
{
	// TODO: handle destination
	NetworkSession::PeerId destinationPeer = 0;
	
	const auto id = systemMessageId++;
	sendMessage(EntityNetworkMessageSystemMsg(messageType, id, std::move(targetSystem), std::move(messageData)), destinationPeer);

	// Only wait for responses from host?
	if (destination == SystemMessageDestination::Host) {
		pendingSysMsgResponses[id] = PendingSysMsgResponse{ std::move(callback) };
		assert(pendingSysMsgResponses.size() < 1000); // Make sure we're not leaking
	}
}

void EntityNetworkSession::setupDictionary()
{
	serializationDictionary.addEntry("components");
	serializationDictionary.addEntry("children");
	serializationDictionary.addEntry("Transform2D");
	serializationDictionary.addEntry("position");
}

World& EntityNetworkSession::getWorld() const
{
	Expects(factory);
	return factory->getWorld();
}

EntityFactory& EntityNetworkSession::getFactory() const
{
	Expects(factory);
	return *factory;
}

const EntityFactory::SerializationOptions& EntityNetworkSession::getEntitySerializationOptions() const
{
	Expects(factory);
	return entitySerializationOptions;
}

const EntityDataDelta::Options& EntityNetworkSession::getEntityDeltaOptions() const
{
	return deltaOptions;
}

const SerializerOptions& EntityNetworkSession::getByteSerializationOptions() const
{
	return byteSerializationOptions;
}

SerializationDictionary& EntityNetworkSession::getSerializationDictionary()
{
	return serializationDictionary;
}

Time EntityNetworkSession::getMinSendInterval() const
{
	return 0.05;
}

void EntityNetworkSession::onRemoteEntityCreated(EntityRef entity, NetworkSession::PeerId peerId)
{
	if (listener) {
		listener->onRemoteEntityCreated(entity, peerId);
	}
}

void EntityNetworkSession::onPreSendDelta(EntityDataDelta& delta)
{
	if (listener) {
		listener->onPreSendDelta(delta);
	}
}

bool EntityNetworkSession::isReadyToStart() const
{
	return readyToStart;
}

bool EntityNetworkSession::isEntityInView(EntityRef entity, const EntityClientSharedData& clientData) const
{
	Expects(listener);
	return listener->isEntityInView(entity, clientData);
}

std::vector<Rect4i> EntityNetworkSession::getRemoteViewPorts() const
{
	std::vector<Rect4i> result;
	for (auto& peer: peers) {
		const auto* data = session->tryGetClientSharedData<EntityClientSharedData>(peer.getPeerId());
		if (data && data->viewRect) {
			result.push_back(data->viewRect.value());
		}
	}
	return result;
}

bool EntityNetworkSession::isHost()
{
	return session->getType() == NetworkSessionType::Host;
}

bool EntityNetworkSession::isRemote(ConstEntityRef entity) const
{
	const auto myId = session->getMyPeerId().value();
	return entity.getOwnerPeerId().value_or(myId) != myId;
}

NetworkSession& EntityNetworkSession::getSession() const
{
	Expects(session);
	return *session;
}

bool EntityNetworkSession::hasWorld() const
{
	return !!factory;
}

void EntityNetworkSession::onStartSession(NetworkSession::PeerId myPeerId)
{
	Logger::logDev("Starting session, I'm peer id " + toString(static_cast<int>(myPeerId)));
	if (myPeerId == 0) {
		readyToStart = true;
	}
	listener->onStartSession(myPeerId);
}

void EntityNetworkSession::onPeerConnected(NetworkSession::PeerId peerId)
{
	peers.emplace_back(*this, peerId);

	Logger::logDev("Peer " + toString(static_cast<int>(peerId)) + " connected to EntityNetworkSession.");
}

void EntityNetworkSession::onPeerDisconnected(NetworkSession::PeerId peerId)
{
	for (auto& peer: peers) {
		if (peer.getPeerId() == peerId) {
			peer.destroy();
		}
	}
	std_ex::erase_if(peers, [](const EntityNetworkRemotePeer& p) { return !p.isAlive(); });

	Logger::logDev("Peer " + toString(static_cast<int>(peerId)) + " disconnected from EntityNetworkSession.");
}

std::unique_ptr<SharedData> EntityNetworkSession::makeSessionSharedData()
{
	return std::make_unique<EntitySessionSharedData>();
}

std::unique_ptr<SharedData> EntityNetworkSession::makePeerSharedData()
{
	return std::make_unique<EntityClientSharedData>();
}

void EntityClientSharedData::serialize(Serializer& s) const
{
	s << viewRect;
}

void EntityClientSharedData::deserialize(Deserializer& s)
{
	s >> viewRect;
}