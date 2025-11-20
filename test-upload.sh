#!/bin/bash

echo "=== Testing Coverity Upload Locally ==="
echo ""

# Check if myproject.zip exists
if [ ! -f "myproject.zip" ]; then
    echo "ERROR: myproject.zip not found!"
    exit 1
fi

# Get file size
FILE_SIZE=$(ls -lh myproject.zip | awk '{print $5}')
echo "Archive size: $FILE_SIZE"

# Check if file is valid zip
if unzip -t myproject.zip >/dev/null 2>&1; then
    echo "✓ Archive is valid"
else
    echo "✗ Archive validation failed"
    exit 1
fi

echo ""
echo "=== Ready to upload ==="
echo "To test upload, you would run:"
echo ""
echo "curl -X POST \\"
echo "  -F \"token=YOUR_COVERITY_TOKEN\" \\"
echo "  -F \"email=cpp.benchmark@proton.me\" \\"
echo "  -F \"file=@myproject.zip\" \\"
echo "  -F \"version=test-$(date +%Y%m%d-%H%M%S)\" \\"
echo "  -F \"description=Manual test upload\" \\"
echo "  \"https://scan.coverity.com/builds?project=cpp-benchmark1/subconverter\""
echo ""
echo "Replace YOUR_COVERITY_TOKEN with the actual token from GitHub secrets."