#pragma once

#include <type_traits>

#include "halley/maths/polygon.h"
#include "halley/data_structures/tree_map.h"
#include "halley/graphics/camera.h"
#include "halley/graphics/sprite/ipainter.h"
#include "halley/maths/colour.h"

namespace Halley {
	class RenderGraphCommand;
	class ScriptState;

	struct DebugLine
	{
		DebugLine(Vector<Vector2f> points, Colour4f colour, float thickness, bool loop)
			: points(std::move(points))
			, colour(colour)
			, thickness(thickness)
			, loop(loop)
		{
		}

		Vector<Vector2f> points;
		Colour4f colour;
		float thickness;
		bool loop;
	};

	struct DebugPoint
	{
		DebugPoint(Vector2f point, Colour4f colour, float radius)
			: point(point)
			, colour(colour)
			, radius(radius)
		{
		}

		Vector2f point;
		Colour4f colour;
		float radius;
	};

	struct DebugPolygon {
		DebugPolygon(Polygon polygon, Colour4f colour)
			: polygon(std::move(polygon))
			, colour(colour)
		{}

		Polygon polygon;
		Colour4f colour;
	};

	struct DebugEllipse {
		DebugEllipse(Vector2f centre, Vector2f radius, float width, Colour4f colour)
			: centre(centre)
			, radius(radius)
			, width(width)
			, colour(colour)
		{}

		Vector2f centre;
		Vector2f radius;
		float width;
		Colour4f colour;
	};

	struct DebugText {
		DebugText(String text = {})
			: text(std::move(text))
		{}

		String text;
		Time time = 0;
	};

	struct DebugWorldText {
		DebugWorldText(String text, Vector2f position)
			: text(std::move(text))
			, position(position)
		{}

		String text;
		Time time = 0;
		Vector2f position;
	};

	class UIWidget;
	class UIRoot;

	class UIRootFrameData {
	public:
		Vector<std::pair<std::shared_ptr<UIWidget>, size_t>> renderWidgets;
		Vector<std::shared_ptr<UIWidget>> renderRoots;
	};

	class BaseFrameData {
	public:
		BaseFrameData();

		virtual ~BaseFrameData() = default;

		static void setThreadFrameData(BaseFrameData* value)
		{
			threadInstance = value;
		}
		
		static BaseFrameData& getCurrent()
		{
			assert(hasCurrent());
			return *threadInstance;
		}

		static bool hasCurrent()
		{
			return threadInstance != nullptr;
		}

		virtual void doStartFrame(bool multithreaded, BaseFrameData* previous, Time deltaTime) {}
		virtual void doEndFrame() {}

		static BaseFrameData& getCurrentBase()
		{
			assert(hasCurrent());
			return *threadInstance;
		}

		template <typename T>
		T* tryGetPainter() const
		{
			for (auto& p: painters) {
				if (auto p2 = dynamic_cast<T*>(p.get())) {
					return p2;
				}
			}
			return nullptr;
		}
		
		int frameIdx = 0;
		Vector<DebugLine> debugLines;
		Vector<DebugPoint> debugPoints;
		Vector<DebugPolygon> debugPolygons;
		Vector<DebugEllipse> debugEllipses;
		Vector<DebugWorldText> debugWorldTexts;
		TreeMap<String, DebugText> debugTexts;
		Vector<std::pair<Vector2f, std::shared_ptr<ScriptState>>> scriptStates;
		HashMap<const UIRoot*, UIRootFrameData> uiRootData;

		Vector<std::unique_ptr<IPainter>> painters;
		Vector<std::pair<String, Camera>> cameras;
		float zoomLevel = 1;

		Camera devCamera;
		Camera devUICamera;

		Vector<std::shared_ptr<RenderGraphCommand>> renderGraphCommands;

	protected:
		static thread_local BaseFrameData* threadInstance;

		void baseStartFrame(bool multithreaded, BaseFrameData* previous, Time deltaTime);
		void baseEndFrame();
	};

	template <typename T>
	class FrameData : public BaseFrameData {
	public:
		virtual void startFrame(bool multithreaded, T* previous, Time deltaTime) {}
		virtual void endFrame() {}

		static T& getCurrent()
		{
			assert(hasCurrent());
			return static_cast<T&>(*threadInstance);
		}

	protected:
		void doStartFrame(bool multithreaded, BaseFrameData* previous, Time deltaTime) override
		{
			baseStartFrame(multithreaded, previous, deltaTime);
			startFrame(multithreaded, static_cast<T*>(previous), deltaTime);
		}

		void doEndFrame() override
		{
			baseEndFrame();
			endFrame();
		}
	};

	class DefaultFrameData : public FrameData<DefaultFrameData> {
	public:
	};
}
