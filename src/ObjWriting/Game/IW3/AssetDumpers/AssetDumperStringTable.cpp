#include "AssetDumperStringTable.h"

#include "Csv/CsvStream.h"

using namespace IW3;

bool AssetDumperStringTable::ShouldDump(XAssetInfo<StringTable>* asset)
{
    return true;
}

bool AssetDumperStringTable::CanDumpAsRaw()
{
    return true;
}

std::string AssetDumperStringTable::GetFileNameForAsset(Zone* zone, XAssetInfo<StringTable>* asset)
{
    return asset->m_name;
}

void AssetDumperStringTable::DumpRaw(AssetDumpingContext& context, XAssetInfo<StringTable>* asset, std::ostream& stream)
{
    const auto* stringTable = asset->Asset();
    CsvOutputStream csv(stream);

    for (auto row = 0; row < stringTable->rowCount; row++)
    {
        for (auto column = 0; column < stringTable->columnCount; column++)
        {
            csv.WriteColumn(stringTable->values[column + row * stringTable->columnCount]);
        }

        csv.NextRow();
    }
}
