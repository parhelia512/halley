#pragma once
#include "halley/file/path.h"
#include <map>
#include <mutex>
#include "halley/text/halleystring.h"
#include <cstdint>
#include <utility>
#include "asset_importer.h"
#include "halley/resources/asset_database.h"

namespace Halley
{
	class Project;
	class Deserializer;
	class Serializer;

	class AssetPath {
	public:
		AssetPath();
		AssetPath(TimestampedPath path);
		AssetPath(TimestampedPath path, Path dataPath);

		const Path& getPath() const;
		const Path& getDataPath() const;
		int64_t getTimestamp() const;

		void serialize(Serializer& s) const;
		void deserialize(Deserializer& s);

	private:
		TimestampedPath path;
		Path dataPath;
	};
	
	class ImportAssetsDatabaseEntry
	{
	public:
		String assetId;
		Path srcDir;
		Vector<AssetPath> inputFiles;
		Vector<TimestampedPath> additionalInputFiles; // These were requested by the importer, rather than enumerated directly
		Vector<AssetResource> outputFiles;
		ImportAssetType assetType = ImportAssetType::Undefined;

		ImportAssetsDatabaseEntry() {}

		ImportAssetsDatabaseEntry(String assetId, Path srcDir, const Path& inputFile, int64_t time)
			: assetId(std::move(assetId))
			, srcDir(std::move(srcDir))
			, inputFiles({ TimestampedPath(inputFile, time) })
		{}

		ImportAssetsDatabaseEntry(String assetId, Path srcDir)
			: assetId(std::move(assetId))
			, srcDir(std::move(srcDir))
		{}

		ImportAssetsDatabaseEntry(String assetId, Path srcDir, Vector<AssetPath>&& inputFiles)
			: assetId(std::move(assetId))
			, srcDir(std::move(srcDir))
			, inputFiles(std::move(inputFiles))
		{}

		void serialize(Serializer& s) const;
		void deserialize(Deserializer& s);
		int64_t getLatestTimestamp() const;
	};

	class ImportAssetsDatabase
	{
		struct AssetEntry
		{
			ImportAssetsDatabaseEntry asset;
			bool present = false;

			void serialize(Serializer& s) const;
			void deserialize(Deserializer& s);
		};

		class InputFileEntry
		{
		public:
			std::array<int64_t, 3> timestamp;
			Metadata metadata;
			Path basePath;
			bool missing = false; // Not serialized

			void serialize(Serializer& s) const;
			void deserialize(Deserializer& s);
		};

	public:
		ImportAssetsDatabase(Path directory, Path dbFile, Path assetsDbFile, Vector<String> platforms, int version);

		void load();
		void save() const;
		std::unique_ptr<AssetDatabase> makeAssetDatabase(const String& platform) const;

		bool needToLoadInputMetadata(const Path& path, std::array<int64_t, 3> timestamps) const;
		void setInputFileMetadata(const Path& path, std::array<int64_t, 3> timestamps, const Metadata& data, Path basePath);
		std::optional<Metadata> getMetadata(const Path& path) const;
		std::optional<Metadata> getMetadata(AssetType type, const String& assetId) const;

		void markInputPresent(const Path& path);
		void markInputMissing(const Path& path);
		void markAllInputFilesAsMissing();
		bool purgeMissingInputs();

		Path getPrimaryInputFile(AssetType type, const String& assetId) const;
		int64_t getAssetTimestamp(AssetType type, const String& assetId) const;

		bool needsImporting(const ImportAssetsDatabaseEntry& asset, bool includeFailed) const;
		void markAsImported(const ImportAssetsDatabaseEntry& asset);
		void markDeleted(const ImportAssetsDatabaseEntry& asset);
		void markFailed(const ImportAssetsDatabaseEntry& asset);
		void markAssetsAsStillPresent(const HashMap<std::pair<ImportAssetType, String>, ImportAssetsDatabaseEntry>& assets);
		Vector<ImportAssetsDatabaseEntry> getAllMissing() const;

		Vector<AssetResource> getOutFiles(ImportAssetType assetType, const String& assetId) const;
		Vector<String> getInputFiles() const;
		Vector<std::pair<AssetType, String>> getAssetsFromFile(const Path& inputFile);

		void serialize(Serializer& s) const;
		void deserialize(Deserializer& s);

	private:
		Vector<String> platforms;
		Path directory;
		Path dbFile;
		Path assetsDbFile;
		const int version;

		std::map<std::pair<ImportAssetType, String>, AssetEntry> assetsImported;
		std::map<std::pair<ImportAssetType, String>, AssetEntry> assetsFailed; // Ephemeral
		std::map<String, InputFileEntry> inputFiles;

		mutable std::map<std::pair<AssetType, String>, const AssetEntry*> assetIndex;
		mutable bool indexDirty = true;
	
		mutable std::mutex mutex;

		const AssetEntry* findEntry(AssetType type, const String& id) const;
		std::unique_ptr<AssetDatabase> doMakeAssetDatabase(const String& platform) const;
	};
}
