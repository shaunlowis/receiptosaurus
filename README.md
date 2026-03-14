# Overview
Goal is to make a web based interface, that I can upload a bunch of photos of receipts to.
I'd like to then get an itemised list of the contents of the receipts.

DISCLAIMER: I am using Claude AI for prompting this into existence!

## Installation
```bash
# Build tools
sudo apt update
sudo apt install -y build-essential cmake pkg-config

# Tesseract + its image library (Leptonica)
sudo apt install -y libtesseract-dev libleptonica-dev

# English language data (required at runtime)
sudo apt install -y tesseract-ocr-eng

# Optional but recommended: better image pre-processing
sudo apt install -y libopencv-dev
```

Additionally, installed more training data to tesseract package:

```bash
# Check that this path is correct:
$ ls /usr/share/tesseract-ocr/5/tessdata/
configs  eng.traineddata  osd.traineddata  pdf.ttf  tessconfigs

# Add more training data:
sudo wget -O /usr/share/tesseract-ocr/5/tessdata/eng.traineddata \
  https://github.com/tesseract-ocr/tessdata_best/raw/main/eng.traineddata
```

## Running

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)

# Normal run
./receipt_ocr ../images/receipt.jpg output.csv

# With debug image (check what Tesseract actually sees)
./receipt_ocr ../images/receipt.jpg output.csv --debug
```