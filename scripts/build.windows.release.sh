#!/bin/bash
set -xe

git clone https://github.com/curl/curl --depth=1 --branch curl-8_6_0
cd curl
cmake -DCMAKE_BUILD_TYPE=Release -DCURL_USE_LIBSSH2=OFF -DHTTP_ONLY=ON -DCURL_USE_SCHANNEL=ON -DBUILD_SHARED_LIBS=OFF -DBUILD_CURL_EXE=OFF -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" -G "Unix Makefiles" -DHAVE_LIBIDN2=OFF -DCURL_USE_LIBPSL=OFF .
make install -j4
cd ..

git clone https://github.com/jbeder/yaml-cpp --depth=1
cd yaml-cpp
cmake -DCMAKE_BUILD_TYPE=Release -DYAML_CPP_BUILD_TESTS=OFF -DYAML_CPP_BUILD_TOOLS=OFF -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" -G "Unix Makefiles" .
make install -j4
cd ..

git clone https://github.com/ftk/quickjspp --depth=1
cd quickjspp
patch quickjs/quickjs-libc.c -i ../scripts/patches/0001-quickjs-libc-add-realpath-for-Windows.patch
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release .
make quickjs -j4
install -d "$MINGW_PREFIX/lib/quickjs/"
install -m644 quickjs/libquickjs.a "$MINGW_PREFIX/lib/quickjs/"
install -d "$MINGW_PREFIX/include/quickjs"
install -m644 quickjs/quickjs.h quickjs/quickjs-libc.h "$MINGW_PREFIX/include/quickjs/"
install -m644 quickjspp.hpp "$MINGW_PREFIX/include/"
cd ..

git clone https://github.com/PerMalmberg/libcron --depth=1
cd libcron
git submodule update --init
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" .
make libcron install -j4
cd ..

echo "Installing rapidjson..."
echo "MINGW_PREFIX: '$MINGW_PREFIX'"

# Create include directory if it doesn't exist
install -d "$MINGW_PREFIX/include"

git clone https://github.com/Tencent/rapidjson --depth=1
cd rapidjson

echo "rapidjson structure:"
ls -la

# rapidjson can have headers in different locations
if [ -d "include" ]; then
    echo "Found include directory, copying headers..."
    cp -r include/* "$MINGW_PREFIX/include/"
elif [ -d "rapidjson" ]; then
    echo "Found rapidjson directory in root, copying..."
    cp -r rapidjson "$MINGW_PREFIX/include/"
else
    echo "❌ Neither include nor rapidjson directory found!"
    echo "Available files:"
    find . -name "*.h" -o -name "*.hpp" | head -10
    exit 1
fi

echo "Verifying rapidjson installation:"
if [ -d "$MINGW_PREFIX/include/rapidjson" ]; then
    echo "✅ rapidjson found in $MINGW_PREFIX/include/rapidjson"
    ls -la "$MINGW_PREFIX/include/rapidjson/" | head -10
elif [ -f "$MINGW_PREFIX/include/rapidjson.h" ]; then
    echo "✅ rapidjson.h found directly in $MINGW_PREFIX/include/"
    ls -la "$MINGW_PREFIX/include/rapidjson.h"
else
    echo "❌ rapidjson NOT found in expected locations"
    echo "Contents of $MINGW_PREFIX/include:"
    ls -la "$MINGW_PREFIX/include/" 2>/dev/null | head -10 || echo "Cannot access MINGW_PREFIX/include"
    exit 1
fi

cd ..

echo "=== TOML11 INSTALLATION ==="
echo "MINGW_PREFIX: '$MINGW_PREFIX'"
echo "Current working directory: $(pwd)"

# Verify MINGW_PREFIX is set and accessible
if [ -z "$MINGW_PREFIX" ]; then
    echo "❌ MINGW_PREFIX is not set!"
    exit 1
fi

if [ ! -d "$MINGW_PREFIX" ]; then
    echo "❌ MINGW_PREFIX directory does not exist: $MINGW_PREFIX"
    echo "Available directories in parent:"
    ls -la "$(dirname "$MINGW_PREFIX")" 2>/dev/null || echo "Cannot access parent directory"
    exit 1
fi

echo "Installing toml11 from GitHub..."
git clone https://github.com/ToruNiina/toml11 --branch v3.8.1 --depth=1
cd toml11

echo "toml11 cloned successfully"
echo "Contents of toml11 directory:"
ls -la

# Check if toml.hpp exists
if [ ! -f "toml.hpp" ]; then
    echo "❌ toml.hpp not found in toml11 directory!"
    ls -la
    exit 1
fi

# Check if toml directory exists
if [ ! -d "toml" ]; then
    echo "❌ toml directory not found in toml11!"
    exit 1
fi

echo "toml directory contents:"
ls -la toml/

# toml11 is header-only, install all headers
echo "Installing toml11 headers to $MINGW_PREFIX/include"
install -d "$MINGW_PREFIX/include"
install -m644 toml.hpp "$MINGW_PREFIX/include/" || {
    echo "❌ Failed to install toml.hpp to $MINGW_PREFIX/include/"
    exit 1
}

install -d "$MINGW_PREFIX/include/toml"
echo "Installing individual headers from toml directory..."
# Install all toml11 headers from the toml directory
for header in toml/*.hpp; do
    echo "Installing $header to $MINGW_PREFIX/include/toml/"
    install -m644 "$header" "$MINGW_PREFIX/include/toml/" || {
        echo "❌ Failed to install $header to $MINGW_PREFIX/include/toml/"
        exit 1
    }
done

echo "✅ toml11 installation completed"
echo "Verifying installation:"
echo "Contents of $MINGW_PREFIX/include:"
ls -la "$MINGW_PREFIX/include/" | head -20

if [ -f "$MINGW_PREFIX/include/toml.hpp" ]; then
    echo "✅ SUCCESS: toml.hpp found in $MINGW_PREFIX/include/"
else
    echo "❌ FAILED: toml.hpp NOT found in $MINGW_PREFIX/include/"
    echo "Checking if directory exists:"
    ls -la "$MINGW_PREFIX/" 2>/dev/null || echo "MINGW_PREFIX directory not accessible"
    exit 1
fi

echo "=== TOML11 INSTALLATION COMPLETE ==="
cd ..

python -m ensurepip
python -m pip install gitpython
python scripts/update_rules.py -c scripts/rules_config.conf

rm -f C:/Strawberry/perl/bin/pkg-config C:/Strawberry/perl/bin/pkg-config.bat
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$MINGW_PREFIX" -G "Unix Makefiles" .
make -j4
rm subconverter.exe
# shellcheck disable=SC2046
g++ $(find CMakeFiles/subconverter.dir/src -name "*.obj") \
    curl/lib/libcurl.a \
    -o base/subconverter.exe \
    -static \
    -lbcrypt -lpcre2-8 \
    -l:quickjs/libquickjs.a \
    -llibcron -lyaml-cpp \
    -lodbc32 \
    -liphlpapi -lcrypt32 -lws2_32 -lwsock32 -lz -lxml2 -llzma -liconv -s
mv base subconverter
