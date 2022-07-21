#include "script_loop.h"

#include "halley/support/logger.h"
using namespace Halley;

ScriptForLoopData::ScriptForLoopData(const ConfigNode& node)
{
	iterations = node.asInt(0);
}

ConfigNode ScriptForLoopData::toConfigNode(const EntitySerializationContext& context)
{
	return ConfigNode(iterations);
}

String ScriptForLoop::getLabel(const ScriptGraphNode& node) const
{
	return toString(node.getSettings()["loopCount"].asInt(0));
}

gsl::span<const IScriptNodeType::PinType> ScriptForLoop::getPinConfiguration(const ScriptGraphNode& node) const
{
	using ET = ScriptNodeElementType;
	using PD = ScriptNodePinDirection;
	const static auto data = std::array<PinType, 3>{ PinType{ ET::FlowPin, PD::Input }, PinType{ ET::FlowPin, PD::Output }, PinType{ ET::FlowPin, PD::Output } };
	return data;
}

Vector<IScriptNodeType::SettingType> ScriptForLoop::getSettingTypes() const
{
	return { SettingType{ "loopCount", "int", Vector<String>{"0"} } };
}

std::pair<String, Vector<ColourOverride>> ScriptForLoop::getNodeDescription(const ScriptGraphNode& node, const World* world, const ScriptGraph& graph) const
{
	const int count = node.getSettings()["loopCount"].asInt(0);
	auto str = ColourStringBuilder(true);
	str.append("Loop ");
	str.append(toString(count), parameterColour);
	str.append(count == 1 ? " time" : " times");
	return str.moveResults();
}

std::pair<String, Vector<ColourOverride>> ScriptForLoop::getPinDescription(const ScriptGraphNode& node, PinType element, ScriptPinId elementIdx) const
{
	if (elementIdx == 1) {
		return {"Flow output after loop", {}};
	} else if (elementIdx == 2) {
		return {"Flow output for each loop iteration", {}};
	} else {
		return ScriptNodeTypeBase<ScriptForLoopData>::getPinDescription(node, element, elementIdx);
	}
}

IScriptNodeType::Result ScriptForLoop::doUpdate(ScriptEnvironment& environment, Time time, const ScriptGraphNode& node, ScriptForLoopData& curData) const
{
	const int count = node.getSettings()["loopCount"].asInt(0);
	const bool done = curData.iterations >= count;
	if (!done) {
		++curData.iterations;
	}
	return Result(ScriptNodeExecutionState::Done, 0, done ? 1 : 2);
}

void ScriptForLoop::doInitData(ScriptForLoopData& data, const ScriptGraphNode& node, const EntitySerializationContext& context, const ConfigNode& nodeData) const
{
	data.iterations = 0;
}

bool ScriptForLoop::doIsStackRollbackPoint(ScriptEnvironment& environment, const ScriptGraphNode& node, ScriptPinId outPin, ScriptForLoopData& curData) const
{
	return outPin == 2;
}

bool ScriptForLoop::canKeepData() const
{
	return true;
}


gsl::span<const IScriptNodeType::PinType> ScriptWhileLoop::getPinConfiguration(const ScriptGraphNode& node) const
{
	using ET = ScriptNodeElementType;
	using PD = ScriptNodePinDirection;
	const static auto data = std::array<PinType, 4>{ PinType{ ET::FlowPin, PD::Input }, PinType{ ET::ReadDataPin, PD::Input }, PinType{ ET::FlowPin, PD::Output }, PinType{ ET::FlowPin, PD::Output } };
	return data;
}

std::pair<String, Vector<ColourOverride>> ScriptWhileLoop::getNodeDescription(const ScriptGraphNode& node, const World* world, const ScriptGraph& graph) const
{
	auto str = ColourStringBuilder(true);
	if (node.getPin(1).hasConnection()) {
		const auto desc = getConnectedNodeName(world, node, graph, 1);
		str.append("Loop as long as ");
		str.append(desc, parameterColour);
		str.append(" is true");
	} else {
		str.append("Loop ");
		str.append("forever", parameterColour);
	}
	return str.moveResults();
}

IScriptNodeType::Result ScriptWhileLoop::doUpdate(ScriptEnvironment& environment, Time time, const ScriptGraphNode& node) const
{
	const bool condition = readDataPin(environment, node, 1).asBool(true);
	return Result(ScriptNodeExecutionState::Done, 0, condition ? 2 : 1);
}

bool ScriptWhileLoop::doIsStackRollbackPoint(ScriptEnvironment& environment, const ScriptGraphNode& node, ScriptPinId outPin) const
{
	return outPin == 3;
}




ScriptLerpLoopData::ScriptLerpLoopData(const ConfigNode& node)
{
	time = node.asFloat(0);
}

ConfigNode ScriptLerpLoopData::toConfigNode(const EntitySerializationContext& context)
{
	return ConfigNode(time);
}

Vector<IScriptNodeType::SettingType> ScriptLerpLoop::getSettingTypes() const
{
	return {
		SettingType{ "time", "float", Vector<String>{"1"} }
	};
}

gsl::span<const IScriptNodeType::PinType> ScriptLerpLoop::getPinConfiguration(const ScriptGraphNode& node) const
{
	using ET = ScriptNodeElementType;
	using PD = ScriptNodePinDirection;
	const static auto data = std::array<PinType, 4>{ PinType{ ET::FlowPin, PD::Input }, PinType{ ET::FlowPin, PD::Output }, PinType{ ET::FlowPin, PD::Output }, PinType{ ET::ReadDataPin, PD::Output } };
	return data;
}

String ScriptLerpLoop::getLabel(const ScriptGraphNode& node) const
{
	return toString(node.getSettings()["time"].asFloat(1)) + "s";
}

std::pair<String, Vector<ColourOverride>> ScriptLerpLoop::getNodeDescription(const ScriptGraphNode& node, const World* world, const ScriptGraph& graph) const
{
	auto str = ColourStringBuilder(true);
	str.append("Loop over ");
	str.append(toString(node.getSettings()["time"].asFloat(1)) + "s", parameterColour);
	str.append(" whilst outputting from 0 to 1");
	return str.moveResults();}

std::pair<String, Vector<ColourOverride>> ScriptLerpLoop::getPinDescription(const ScriptGraphNode& node, PinType element, ScriptPinId elementIdx) const
{
	if (elementIdx == 1) {
		return {"Flow output after loop", {}};
	} else if (elementIdx == 2) {
		return {"Flow output for each loop iteration", {}};
	} else if (elementIdx == 3) {
		return {"Loop progress (0..1)", {}};
	} else {
		return ScriptNodeTypeBase<ScriptLerpLoopData>::getPinDescription(node, element, elementIdx);
	}
}

String ScriptLerpLoop::getShortDescription(const World* world, const ScriptGraphNode& node, const ScriptGraph& graph, ScriptPinId element_idx) const
{
	return "Lerp progress";
}

void ScriptLerpLoop::doInitData(ScriptLerpLoopData& data, const ScriptGraphNode& node, const EntitySerializationContext& context, const ConfigNode& nodeData) const
{
	data = ScriptLerpLoopData(nodeData);
}

IScriptNodeType::Result ScriptLerpLoop::doUpdate(ScriptEnvironment& environment, Time time, const ScriptGraphNode& node, ScriptLerpLoopData& curData) const
{
	// Important: check for done before incrementing time. This makes sure that we had at least one iteration outputting 1.0f before terminating
	const float length = node.getSettings()["time"].asFloat(1);
	const bool done = curData.time >= length;
	const float timeLeft = std::max(length - curData.time, 0.0f);
	curData.time += static_cast<float>(time);
	return Result(ScriptNodeExecutionState::Done, std::min(static_cast<Time>(timeLeft), time), done ? 1 : 2);
}

ConfigNode ScriptLerpLoop::doGetData(ScriptEnvironment& environment, const ScriptGraphNode& node, size_t pinN, ScriptLerpLoopData& curData) const
{
	const float length = node.getSettings()["time"].asFloat(1);
	return ConfigNode(clamp(curData.time / length, 0.0f, 1.0f));
}

bool ScriptLerpLoop::doIsStackRollbackPoint(ScriptEnvironment& environment, const ScriptGraphNode& node, ScriptPinId outPin, ScriptLerpLoopData& curData) const
{
	return outPin == 2;
}



std::pair<String, Vector<ColourOverride>> ScriptWhileLoop::getPinDescription(const ScriptGraphNode& node, PinType element, ScriptPinId elementIdx) const
{
	if (elementIdx == 1) {
		return {"Condition", {}};
	} else if (elementIdx == 2) {
		return {"Flow output after loop", {}};
	} else if (elementIdx == 3) {
		return {"Flow output for each loop iteration", {}};
	} else {
		return ScriptNodeTypeBase<void>::getPinDescription(node, element, elementIdx);
	}
}




ScriptEveryFrameData::ScriptEveryFrameData(const ConfigNode& node)
{
	lastFrameN = node["lastFrameN"].asInt();
}

ConfigNode ScriptEveryFrameData::toConfigNode(const EntitySerializationContext& context)
{
	ConfigNode::MapType result;
	result["lastFrameN"] = lastFrameN;
	return result;
}

gsl::span<const IScriptNodeType::PinType> ScriptEveryFrame::getPinConfiguration(const ScriptGraphNode& node) const
{
	using ET = ScriptNodeElementType;
	using PD = ScriptNodePinDirection;
	const static auto data = std::array<PinType, 3>{ PinType{ ET::FlowPin, PD::Input }, PinType{ ET::FlowPin, PD::Output }, PinType{ ET::ReadDataPin, PD::Output } };
	return data;
}

std::pair<String, Vector<ColourOverride>> ScriptEveryFrame::getNodeDescription(const ScriptGraphNode& node, const World* world, const ScriptGraph& graph) const
{
	auto str = ColourStringBuilder(true);
	str.append("Pulse every ");
	str.append("frame", parameterColour);
	return str.moveResults();
}

std::pair<String, Vector<ColourOverride>> ScriptEveryFrame::getPinDescription(const ScriptGraphNode& node, PinType element, ScriptPinId elementIdx) const
{
	if (elementIdx == 2) {
		return { "Frame delta time", {} };
	} else {
		return ScriptNodeTypeBase<ScriptEveryFrameData>::getPinDescription(node, element, elementIdx);
	}
}

String ScriptEveryFrame::getShortDescription(const World* world, const ScriptGraphNode& node, const ScriptGraph& graph, ScriptPinId elementIdx) const
{
	return "Frame delta time";
}

IScriptNodeType::Result ScriptEveryFrame::doUpdate(ScriptEnvironment& environment, Time time, const ScriptGraphNode& node, ScriptEveryFrameData& curData) const
{
	const auto curFrame = environment.getCurrentFrameNumber();
	if (curFrame != curData.lastFrameN) {
		curData.lastFrameN = curFrame;
		return Result(ScriptNodeExecutionState::Fork, 0, 1);
	} else {
		return Result(ScriptNodeExecutionState::Executing, time);
	}
}

ConfigNode ScriptEveryFrame::doGetData(ScriptEnvironment& environment, const ScriptGraphNode& node, size_t pinN, ScriptEveryFrameData& curData) const
{
	return ConfigNode(static_cast<float>(environment.getDeltaTime()));
}

void ScriptEveryFrame::doInitData(ScriptEveryFrameData& data, const ScriptGraphNode& node, const EntitySerializationContext& context, const ConfigNode& nodeData) const
{
	data.lastFrameN = -1;
}

bool ScriptEveryFrame::canKeepData() const
{
	return true;
}



ScriptEveryTimeData::ScriptEveryTimeData(const ConfigNode& node)
{
	time = node["time"].asFloat(0);
}

ConfigNode ScriptEveryTimeData::toConfigNode(const EntitySerializationContext& context)
{
	ConfigNode::MapType result;
	result["time"] = time;
	return result;
}

String ScriptEveryTime::getLabel(const ScriptGraphNode& node) const
{
	return toString(node.getSettings()["time"].asFloat(1.0f)) + " s";
}

Vector<IScriptNodeType::SettingType> ScriptEveryTime::getSettingTypes() const
{
	return { SettingType{ "time", "float", Vector<String>{"1"} } };
}

gsl::span<const IScriptNodeType::PinType> ScriptEveryTime::getPinConfiguration(const ScriptGraphNode& node) const
{
	using ET = ScriptNodeElementType;
	using PD = ScriptNodePinDirection;
	const static auto data = std::array<PinType, 2>{ PinType{ ET::FlowPin, PD::Input }, PinType{ ET::FlowPin, PD::Output } };
	return data;
}

std::pair<String, Vector<ColourOverride>> ScriptEveryTime::getNodeDescription(const ScriptGraphNode& node, const World* world, const ScriptGraph& graph) const
{
	auto str = ColourStringBuilder(true);
	str.append("Pulse every ");
	str.append(toString(node.getSettings()["time"].asFloat(1.0f)), parameterColour);
	str.append(" s");
	return str.moveResults();
}

IScriptNodeType::Result ScriptEveryTime::doUpdate(ScriptEnvironment& environment, Time time, const ScriptGraphNode& node, ScriptEveryTimeData& curData) const
{
	const float period = node.getSettings()["time"].asFloat(1.0f);
	const float timeToNextPulse = period - curData.time;
	if (timeToNextPulse < static_cast<float>(time)) {
		curData.time = 0;
		return Result(ScriptNodeExecutionState::Fork, static_cast<Time>(timeToNextPulse), 1);
	} else {
		curData.time += static_cast<float>(time);
		return Result(ScriptNodeExecutionState::Executing, time);
	}
}

void ScriptEveryTime::doInitData(ScriptEveryTimeData& data, const ScriptGraphNode& node, const EntitySerializationContext& context, const ConfigNode& nodeData) const
{
	data.time = node.getSettings()["time"].asFloat(1.0f);
}

bool ScriptEveryTime::canKeepData() const
{
	return true;
}
