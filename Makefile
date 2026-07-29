# ======================================================================
#  Monica NFC Reader GUI - Makefile
# ======================================================================

TARGET  := Monica_NFC_Reader_GUI.exe
BLD_DIR := build
SRC_DIR := src

# ---- Qt 动态版本检测 ----
IS_QT6 := $(shell pkg-config --exists Qt6Core 2>/dev/null && echo 1 || echo 0)
IS_QT5 := $(shell pkg-config --exists Qt5Core 2>/dev/null && echo 1 || echo 0)

ifeq ($(IS_QT6),1)
    QT_PKG := Qt6Widgets Qt6Core Qt6Gui Qt6Network
    MOC := $(shell pkg-config --variable=libexecdir Qt6Core 2>/dev/null)/moc
else ifeq ($(IS_QT5),1)
    QT_PKG := Qt5Widgets Qt5Core Qt5Gui Qt5Network
    MOC    := $(shell which moc 2>/dev/null || which moc-qt5 2>/dev/null)
else
    $(error "Qt not found")
endif

# ---- 编译器 ----
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pipe
LDFLAGS  :=

CXXFLAGS += $(shell pkg-config --cflags $(QT_PKG))
LIBS     := $(shell pkg-config --libs $(QT_PKG))

HAS_HIDAPI := $(shell pkg-config --exists hidapi 2>/dev/null && echo 1 || echo 0)
ifeq ($(HAS_HIDAPI),1)
    CXXFLAGS += $(shell pkg-config --cflags hidapi)
    LIBS     += $(shell pkg-config --libs hidapi)
else
    $(error "hidapi not found")
endif

# ---- 源文件 ----
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BLD_DIR)/%.o,$(SRCS))
MOCS := $(BLD_DIR)/moc_mainwindow.cpp $(BLD_DIR)/moc_hidworker.cpp
MOC_OBJS := $(patsubst $(BLD_DIR)/%.cpp,$(BLD_DIR)/%.o,$(MOCS))
ALL_OBJS := $(OBJS) $(MOC_OBJS)

# ======================================================================
.PHONY: all clean deploy pack

all: $(BLD_DIR) $(TARGET)

$(BLD_DIR):
	mkdir -p $(BLD_DIR)

$(TARGET): $(ALL_OBJS)
	@echo "[LD] $(TARGET)"
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BLD_DIR)/moc_%.cpp: $(SRC_DIR)/%.h | $(BLD_DIR)
	@echo "[MOC] $<"
	$(MOC) $< -o $@

$(BLD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BLD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BLD_DIR)/moc_%.o: $(BLD_DIR)/moc_%.cpp | $(BLD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BLD_DIR)/mainwindow.o: $(BLD_DIR)/moc_mainwindow.cpp
$(BLD_DIR)/hidworker.o:    $(BLD_DIR)/moc_hidworker.cpp

clean:
	rm -rf $(BLD_DIR) $(TARGET)

# ======================================================================
#  部署: 递归收集 DLL → deploy/ 文件夹
# ======================================================================
deploy: $(TARGET)
	@echo "[DEPLOY]"
	rm -rf deploy
	mkdir -p deploy
	cp $(TARGET) deploy/
	windeployqt6 --no-translations --no-compiler-runtime --no-system-d3d-compiler deploy/$(TARGET) 2>/dev/null || true
	bash copy-deps.sh deploy/$(TARGET) deploy/
	cp /c/msys64/mingw64/bin/libiconv-2.dll deploy/ 2>/dev/null || true
	@echo "[DEPLOY] Done → deploy/"

# ======================================================================
#  打包: 7z 自解压单文件 EXE
# ======================================================================
pack: deploy
	@echo "[PACK]"
	cp "/c/Program Files/7-Zip/7z.sfx" deploy/7zS.sfx 2>/dev/null || \
	cp "/c/Program Files/7-Zip/7zS.sfx" deploy/7zS.sfx 2>/dev/null
	echo ';!@Install@!UTF-8!' > deploy/config.txt
	echo 'Title="Monica NFC Reader GUI"' >> deploy/config.txt
	echo 'BeginPrompt="Install Monica NFC Reader GUI?"' >> deploy/config.txt
	echo 'Directory="Monica NFC Reader"' >> deploy/config.txt
	echo 'ExtractPathText="Select folder:"' >> deploy/config.txt
	echo 'ExecuteFile="Monica_NFC_Reader_GUI.exe"' >> deploy/config.txt
	echo ';!@InstallEnd@!' >> deploy/config.txt
	cd deploy && "/c/Program Files/7-Zip/7z.exe" a -sfx7zS.sfx ../MonicaNFCReader_Setup.exe * -x!config.txt -x!7zS.sfx > /dev/null
	rm -f deploy/7zS.sfx deploy/config.txt
	@echo "[PACK] Done → MonicaNFCReader_Setup.exe"
	@ls -lh MonicaNFCReader_Setup.exe
