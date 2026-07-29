# ======================================================================
#  NFC Card Reader - Makefile
#
#  构建:  make
#  清理:  make clean
#  运行:  make run
#
#  依赖:  Qt 5.15+ 或 Qt 6.x  (推荐 Qt 6.x)
#         hidapi  (MSYS2: pacman -S mingw-w64-x86_64-hidapi)
#         GCC (MinGW / MSYS2)
#         pkg-config
#
#  如 pkg-config 不可用, 可手动设置:
#    make QTDIR=/path/to/qt MOC=/path/to/moc
# ======================================================================

# ---- Qt 版本检测 ----
IS_QT6 := $(shell pkg-config --exists Qt6Core 2>/dev/null && echo 1 || echo 0)
IS_QT5 := $(shell pkg-config --exists Qt5Core 2>/dev/null && echo 1 || echo 0)

ifeq ($(IS_QT6),1)
    QT_PKG := Qt6Widgets Qt6Core Qt6Gui Qt6Network
    # moc 位置: pkg-config libexecdir 直接指向 share/qt6/bin/
    MOC := $(shell pkg-config --variable=libexecdir Qt6Core 2>/dev/null)/moc
    ifeq ($(shell test -x "$(MOC)" || echo no),no)
        $(error "moc not found at $(MOC). Set MOC=/path/to/moc manually.")
    endif
else ifeq ($(IS_QT5),1)
    QT_PKG := Qt5Widgets Qt5Core Qt5Gui Qt5Network
    MOC    := $(shell which moc 2>/dev/null || which moc-qt5 2>/dev/null)
else
    ifdef QTDIR
        # 手动指定 QTDIR 时的回退
        QT_PKG :=
        MOC    := $(QTDIR)/bin/moc
        CUSTOM_QT := 1
    else
        $(error "Qt not found. Install Qt6 or Qt5 dev packages, or set QTDIR=/path/to/qt")
    endif
endif

# ---- 编译器和标志 ----
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pipe
LDFLAGS  :=

# ---- HIDAPI ----
HAS_HIDAPI := $(shell pkg-config --exists hidapi 2>/dev/null && echo 1 || echo 0)
ifeq ($(HAS_HIDAPI),1)
    CXXFLAGS += $(shell pkg-config --cflags hidapi)
    LIBS_HID  := $(shell pkg-config --libs hidapi)
else
    $(error "hidapi not found. Install with: pacman -S mingw-w64-x86_64-hidapi")
endif

ifndef CUSTOM_QT
    CXXFLAGS += $(shell pkg-config --cflags $(QT_PKG))
    LIBS     := $(shell pkg-config --libs $(QT_PKG))
else
    # 手动 QTDIR 模式
    CXXFLAGS += -I$(QTDIR)/include \
                -I$(QTDIR)/include/QtWidgets \
                -I$(QTDIR)/include/QtCore \
                -I$(QTDIR)/include/QtGui
    LIBS     := -L$(QTDIR)/lib -lQt6Widgets -lQt6Core -lQt6Gui
endif

# 最终链接库
LIBS += $(LIBS_HID)

# ---- 路径 ----
SRC_DIR   := src
BLD_DIR   := build
TARGET    := Monica_NFC_Reader_GUI.exe

# ---- 源文件 ----
CPP_SRCS  := $(wildcard $(SRC_DIR)/*.cpp)
CPP_OBJS  := $(patsubst $(SRC_DIR)/%.cpp,$(BLD_DIR)/%.o,$(CPP_SRCS))

# 需要 moc 处理的头文件 (含有 Q_OBJECT 宏)
MOC_HDRS  := $(SRC_DIR)/mainwindow.h $(SRC_DIR)/hidworker.h
MOC_SRCS  := $(patsubst $(SRC_DIR)/%.h,$(BLD_DIR)/moc_%.cpp,$(MOC_HDRS))
MOC_OBJS  := $(MOC_SRCS:.cpp=.o)

# 所有目标文件
ALL_OBJS  := $(CPP_OBJS) $(MOC_OBJS)

# ======================================================================
#  目标
# ======================================================================
.PHONY: all clean run

all: $(BLD_DIR) $(TARGET)

# 创建 build 目录
$(BLD_DIR):
	@mkdir -p $(BLD_DIR)

# 链接
$(TARGET): $(ALL_OBJS)
	@echo "[LD] $(TARGET)"
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

# ---- moc 规则: 从 .h 生成 moc_xxx.cpp ----
$(BLD_DIR)/moc_%.cpp: $(SRC_DIR)/%.h | $(BLD_DIR)
	@echo "[MOC] $<"
	$(MOC) $< -o $@

# ---- 编译 .cpp → .o (源文件) ----
$(BLD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BLD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ---- 编译 .cpp → .o (moc 生成的文件) ----
$(BLD_DIR)/moc_%.o: $(BLD_DIR)/moc_%.cpp | $(BLD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ---- 依赖关系 (确保 moc 文件先生成) ----
$(BLD_DIR)/mainwindow.o: $(BLD_DIR)/moc_mainwindow.cpp
$(BLD_DIR)/hidworker.o:    $(BLD_DIR)/moc_hidworker.cpp

# ======================================================================
#  辅助目标
# ======================================================================
clean:
	@echo "[CLEAN] Removing build artifacts..."
	rm -rf $(BLD_DIR) $(TARGET)

run: $(TARGET)
	./$(TARGET)

# 打印诊断信息
info:
	@echo "Qt6 detected: $(IS_QT6)"
	@echo "Qt5 detected: $(IS_QT5)"
	@echo "HIDAPI found: $(HAS_HIDAPI)"
	@echo "MOC:          $(MOC)"
	@echo "CXX:          $(CXX)"
	@echo "Target:       $(TARGET)"
	@echo "Sources:      $(CPP_SRCS)"
	@echo "MOC headers:  $(MOC_HDRS)"
	@echo "Objects:      $(ALL_OBJS)"
	@echo "LIBS:         $(LIBS)"
