#!/bin/bash
# Script wrapper to generate cli-diagnostics.json
# Compatible with the Coverity MSYS2 environment

set -e

echo "🔍 Generating Coverity diagnostics..."

# Check if in the correct directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Error: CMakeLists.txt not found. Please run from project root."
    exit 1
fi

# Check if Coverity has already executed the build
if [ -f "cov-int/build-log.txt" ]; then
    echo "📋 Coverity build detected, analyzing logs..."
else
    echo "⚠️ Warning: No Coverity build logs found. This script works best after Coverity analysis."
fi

# Generate cli-diagnostics.json using Coverity logs
echo "📋 Generating cli-diagnostics.json..."
python3 scripts/generate_cli_diagnostics.py --verbose

echo "✅ Coverity diagnostics generated successfully!"
echo "📁 Files created:"
echo "   - cli-diagnostics.json (in root and cov-int/output/)"

if [ -f "build/compile_commands.json" ]; then
    echo "   - compile_commands.json (in build/ directory)"
fi