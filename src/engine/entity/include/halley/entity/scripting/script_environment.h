#pragma once
#include "script_node_type.h"

namespace Halley {
    class ScriptState;
	
    class ScriptEnvironment {
    public:
    	ScriptEnvironment(const HalleyAPI& api, World& world, Resources& resources, const ScriptNodeTypeCollection& nodeTypeCollection);
    	virtual ~ScriptEnvironment() = default;

    	virtual void update(Time time, ScriptState& graphState, EntityId curEntity);
    	void terminateState(ScriptState& graphState, EntityId curEntity);
        void abortCodePath(ScriptNodeId node, std::optional<ScriptPinId> outputPin);

    	EntityRef tryGetEntity(EntityId entityId);
    	const ScriptGraph* getCurrentGraph() const;
        size_t& getNodeCounter(uint32_t nodeId);

    	void playMusic(const String& music, float fadeTime);
    	void stopMusic(float fadeTime);

    	virtual ConfigNode getVariable(const String& variable);
    	virtual void setVariable(const String& variable, ConfigNode data);

    	virtual void setDirection(EntityId entityId, const String& direction);

    protected:
		const HalleyAPI& api;
    	World& world;
    	Resources& resources;
    	const ScriptNodeTypeCollection& nodeTypeCollection;

    	const ScriptGraph* currentGraph = nullptr;
    	ScriptState* currentState = nullptr;
        EntityId currentEntity;

    private:
        void updateThread(ScriptState& graphState, ScriptStateThread& thread, Vector<ScriptStateThread>& pendingThreads);
        void doTerminateState();
        void addThread(ScriptStateThread thread, Vector<ScriptStateThread>& pending);
        void advanceThread(ScriptStateThread& thread, OptionalLite<ScriptNodeId> node, ScriptPinId outputPin);
        void forkThread(ScriptStateThread& thread, std::array<IScriptNodeType::OutputNode, 8> outputNodes, Vector<ScriptStateThread>& pendingThreads, size_t firstIdx = 0);
        void mergeThread(ScriptStateThread& thread, bool wait);
        void terminateThread(ScriptStateThread& thread);
        void removeStoppedThreads();
    };
}
