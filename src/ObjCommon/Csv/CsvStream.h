#pragma once
#include "Utils/MemoryManager.h"

#include <functional>
#include <iostream>
#include <string>
#include <vector>

class CsvInputStream
{
public:
    explicit CsvInputStream(std::istream& stream);

    bool NextRow(std::vector<std::string>& out) const;
    bool NextRow(std::vector<const char*>& out, MemoryManager& memory) const;

private:
    bool EmitNextRow(const std::function<void(std::string)>& cb) const;

    std::istream& m_stream;
};

class CsvOutputStream
{
public:
    explicit CsvOutputStream(std::ostream& stream);

    void WriteColumn(const std::string& value);
    void NextRow();

private:
    std::ostream& m_stream;
    unsigned m_column_count;
    unsigned m_current_column;
    bool m_first_row;
};
