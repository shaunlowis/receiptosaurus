#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include "preprocess.h"           // ← add this
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <regex>
#include <string>

// Represents a single line item parsed from the receipt
struct LineItem {
    std::string description;
    std::string price;
    std::string rawLine;  // ← add this
};

// Trim whitespace from both ends
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// Try to parse a receipt line into a description + price.
// Handles formats like:
//   "Milk 2%              $3.49"
//   "SUBTOTAL             12.50"
std::vector<LineItem> parseReceiptText(const std::string& rawText) {
    std::vector<LineItem> items;

    // Matches $ followed by anything vaguely price-like (including garbled chars)
    std::regex pricePattern(R"(\$[\w\s,.;]{1,10})");

    std::istringstream stream(rawText);
    std::string line;
    while (std::getline(stream, line)) {
        // Normalise whitespace
        line = std::regex_replace(line, std::regex(R"(\s+)"), " ");
        line = trim(line);
        if (line.empty()) continue;

        LineItem item;
        item.rawLine = line;

        std::smatch match;
        if (std::regex_search(line, match, pricePattern)) {  // search, not match
            item.price       = trim(match[0].str());         // [0] = full match
            item.description = trim(line.substr(0, match.position()));
            if (item.description.empty())
                item.description = line;
        } else {
            // No price found — keep the line as description-only
            item.description = line;
        }

        items.push_back(item);
    }
    return items;
}

// Escape a CSV field (wrap in quotes if it contains comma/quote/newline)
std::string csvEscape(const std::string& field) {
    if (field.find_first_of(",\"\n") != std::string::npos) {
        std::string escaped = "\"";
        for (char c : field) {
            if (c == '"') escaped += "\"\"";
            else escaped += c;
        }
        escaped += "\"";
        return escaped;
    }
    return field;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <image_path> <output.csv> [--debug]\n";
        return 1;
    }

    const char* imagePath  = argv[1];
    const char* outputPath = argv[2];
    bool debug = (argc >= 4 && std::string(argv[3]) == "--debug");

    // --- 1. Pre-process image with OpenCV ---
    Pix* image = nullptr;
    try {
        image = preprocessReceipt(imagePath, debug);
        if (debug) std::cout << "Debug image saved to debug_preprocessed.png\n";
    } catch (const std::exception& e) {
        std::cerr << "Preprocessing error: " << e.what() << "\n";
        return 1;
    }

    // --- 2. Run Tesseract OCR ---
    tesseract::TessBaseAPI tess;
    // Point explicitly at the 'best' tessdata
    if (tess.Init("/usr/share/tesseract-ocr/5/tessdata/", "eng",
                tesseract::OEM_LSTM_ONLY) != 0) {
        std::cerr << "Error: Could not initialize Tesseract.\n";
        pixDestroy(&image);
        return 1;
    }

    // PSM_SINGLE_BLOCK works well for receipts (columnar text)
    tess.SetPageSegMode(tesseract::PSM_SINGLE_COLUMN);
    tess.SetImage(image);

    // Tell Tesseract to ignore thin image fragments (kills the "too small" warnings)
    tess.SetVariable("tessedit_min_acceptable_image_width", "10");
    // Helps LSTM handle the dot-matrix thermal font spacing
    tess.SetVariable("lstm_choice_mode", "2");

    // GetUTF8Text allocates; we wrap in unique_ptr for auto-free
    std::unique_ptr<char[]> rawText(tess.GetUTF8Text());
    if (!rawText) {
        std::cerr << "Error: OCR returned no text.\n";
        tess.End();
        pixDestroy(&image);
        return 1;
    }

    std::cout << "--- Raw OCR output ---\n" << rawText.get() << "\n";

    // --- 3. Parse into line items ---
    std::vector<LineItem> items = parseReceiptText(rawText.get());

    // --- 4. Write CSV ---
    std::ofstream csv(outputPath);
    if (!csv.is_open()) {
        std::cerr << "Error: Could not open output file: " << outputPath << "\n";
        return 1;
    }

    csv << "Description,Price,Raw OCR Line\n";
    for (const auto& item : items) {
        csv << csvEscape(item.description) << ","
            << csvEscape(item.price)       << ","   // raw, no validation
            << csvEscape(item.rawLine)     << "\n";
    }

    std::cout << "Wrote " << items.size() << " items to " << outputPath << "\n";

    // --- 5. Cleanup ---
    tess.End();
    pixDestroy(&image);
    return 0;
}