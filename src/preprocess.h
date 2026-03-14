#pragma once
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <string>

// Converts an OpenCV Mat (after processing) into a Leptonica Pix*
// so Tesseract can consume it directly — avoids writing a temp file to disk.
inline Pix* matToPix(const cv::Mat& mat) {
    CV_Assert(mat.type() == CV_8UC1); // must be greyscale at this point
    Pix* pix = pixCreate(mat.cols, mat.rows, 8);
    for (int y = 0; y < mat.rows; y++) {
        for (int x = 0; x < mat.cols; x++) {
            pixSetPixel(pix, x, y, mat.at<uint8_t>(y, x));
        }
    }
    return pix;
}

// Full preprocessing pipeline for receipt images.
// Returns a Leptonica Pix* ready to hand to Tesseract.
inline Pix* preprocessReceipt(const std::string& imagePath, bool saveDebug = false) {

    // --- Step 1: Load ---
    cv::Mat src = cv::imread(imagePath, cv::IMREAD_COLOR);
    if (src.empty())
        throw std::runtime_error("Could not load image: " + imagePath);

    // --- Step 2: Greyscale ---
    cv::Mat grey;
    cv::cvtColor(src, grey, cv::COLOR_BGR2GRAY);

    // --- Step 3: Upscale if the image is small ---
    // Tesseract performs best at ~300 DPI. Upscaling tiny images helps a lot.
    if (grey.cols < 1000) {
        double scale = 1000.0 / grey.cols;
        cv::resize(grey, grey, cv::Size(), scale, scale, cv::INTER_CUBIC);
    }

    // --- Step 4: CLAHE (contrast-limited adaptive histogram equalisation) ---
    // This is the key step for faded/uneven thermal receipts.
    // It boosts local contrast without blowing out already-dark areas.
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->setClipLimit(3.0);           // lower = subtler boost; raise to 4–5 for very faded receipts
    clahe->setTilesGridSize({8, 8});    // smaller tiles = more localised correction
    cv::Mat enhanced;
    clahe->apply(grey, enhanced);

    // --- Step 5: Mild Gaussian blur to reduce noise before thresholding ---
    cv::Mat blurred;
    cv::GaussianBlur(enhanced, blurred, cv::Size(3, 3), 0);

    // --- Step 6: Adaptive threshold → clean black-and-white ---
    // adaptiveThreshold handles uneven lighting far better than a global threshold.
    // blockSize (51): the neighbourhood size — increase for larger/bolder fonts
    // C (10): constant subtracted from mean — increase to make more pixels white
    cv::Mat binary;
    cv::adaptiveThreshold(
        blurred, binary,
        255,
        cv::ADAPTIVE_THRESH_GAUSSIAN_C,
        cv::THRESH_BINARY,
        51,   // blockSize — must be odd
        10    // C
    );

    // --- Step 7: Morphological opening to remove isolated speckles ---
    // This cleans up the tiny dots that confuse Tesseract into seeing extra characters.
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));
    cv::Mat clean;
    cv::morphologyEx(binary, clean, cv::MORPH_OPEN, kernel);

    // --- Step 8: Optional — save debug image so you can see what Tesseract sees ---
    if (saveDebug)
        cv::imwrite("debug_preprocessed.png", clean);

    return matToPix(clean);
}