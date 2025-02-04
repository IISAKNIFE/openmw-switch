#include "esmloader.hpp"
#include "esmstore.hpp"

#include <components/esm3/esmreader.hpp>
#include <components/esm3/readerscache.hpp>
#include <components/files/conversion.hpp>

namespace MWWorld
{

    EsmLoader::EsmLoader(MWWorld::ESMStore& store, ESM::ReadersCache& readers, ToUTF8::Utf8Encoder* encoder,
        std::vector<int>& esmVersions)
        : mReaders(readers)
        , mStore(store)
        , mEncoder(encoder)
        , mDialogue(nullptr) // A content file containing INFO records without a DIAL record appends them to the
                             // previous file's dialogue
        , mESMVersions(esmVersions)
    {
    }

    void EsmLoader::load(const std::filesystem::path& filepath, int& index, Loading::Listener* listener)
    {
        const ESM::ReadersCache::BusyItem reader = mReaders.get(static_cast<std::size_t>(index));

        reader->setEncoder(mEncoder);
        reader->setIndex(index);
        reader->open(filepath);
        reader->resolveParentFileIndices(mReaders);

        assert(reader->getGameFiles().size() == reader->getParentFileIndices().size());
        for (std::size_t i = 0, n = reader->getParentFileIndices().size(); i < n; ++i)
            if (i == static_cast<std::size_t>(reader->getIndex()))
                throw std::runtime_error("File " + Files::pathToUnicodeString(reader->getName()) + " asks for parent file "
                + reader->getGameFiles()[i].name
                + ", but it is not available or has been loaded in the wrong order. "
                  "Please run the launcher to fix this issue.");

        mESMVersions[index] = reader->getVer();
        mStore.load(*reader, listener, mDialogue);

        if (!mMasterFileFormat.has_value()
            && (Misc::StringUtils::ciEndsWith(reader->getName().u8string(), u8".esm")
                || Misc::StringUtils::ciEndsWith(reader->getName().u8string(), u8".omwgame")))
            mMasterFileFormat = reader->getFormat();
    }

} /* namespace MWWorld */
