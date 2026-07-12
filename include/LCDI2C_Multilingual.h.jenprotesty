#ifndef LCDI2C_MULTILINGUAL_STUB_H
#define LCDI2C_MULTILINGUAL_STUB_H

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

// Minimal stub for testing: simulates an LCD and prints its full
//  contents to the Serial console after each write.

class LCDI2C_Latin {
public:
    LCDI2C_Latin(int address, int width, int height)
        : addr(address), cols(width), rows(height), curCol(0), curRow(0) {
        buffer.assign(rows, std::string(cols, ' '));
    }

    void init() {
        clear();
    }

    void backlight() {
        // no-op for stub
    }

    void setTextSize(int) {}
    void setTextColor(int) {}

    void setCursor(int col, int row) {
        if (col < 0) col = 0;
        if (row < 0) row = 0;
        if (row >= rows) row = rows - 1;
        if (col >= cols) col = cols - 1;
        curCol = col;
        curRow = row;
    }

    // print overload for C-strings
    void print(const char* s) {
        writeString(std::string(s));
    }

    // print overload for std::string
    void print(const std::string& s) {
        writeString(s);
    }

    // generic print using streamable types
    template<typename T>
    void print(const T& v) {
        std::ostringstream ss;
        ss << v;
        writeString(ss.str());
    }

private:
    int addr;
    int cols;
    int rows;
    int curCol;
    int curRow;
    std::vector<std::string> buffer;

    void clear() {
        for (int r = 0; r < rows; ++r) buffer[r].assign(cols, ' ');
        dumpDisplay();
    }

    void writeString(const std::string& s) {
        for (char ch : s) {
            if (curCol >= cols) break;
            buffer[curRow][curCol++] = ch;
        }
        dumpDisplay();
    }

    void dumpDisplay() {
        // Print display contents to stdout to simulate Serial console output
        std::cout << "+- LCD DUMP (" << cols << "x" << rows << ") -+" << std::endl;
        for (int r = 0; r < rows; ++r) {
            std::cout << '|' + buffer[r] + '|' << std::endl;
        }
        std::cout << "+--------------------+" << std::endl;
    }
};

#endif // LCDI2C_MULTILINGUAL_STUB_H
