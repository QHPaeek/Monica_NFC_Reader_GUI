# ======================================================================
#  Monica NFC Reader GUI — Makefile (qmake 静态编译, 8 核)
# ======================================================================

TARGET  := Monica_NFC_Reader_GUI.exe
BLD     := build
QMAKE   := C:/msys64/mingw64/qt6-static/bin/qmake6.exe

.PHONY: all clean

all: $(TARGET)

$(TARGET): MonicaNFCReader.pro $(shell find src -name '*.cpp' -o -name '*.h')
	@echo "[QMAKE] Generating static Makefile..."
	$(QMAKE) MonicaNFCReader.pro -o $(BLD)/Makefile CONFIG+=release
	# 修补链接顺序: harfbuzz 依赖 graphite2, freetype 依赖 bz2
	sed -i 's/^LIBS[[:space:]]*=[[:space:]]*/& -Wl,--start-group /' $(BLD)/Makefile.Release
	sed -i 's/ -lshell32 $$/ -lshell32 -lgraphite2 -lbz2 -lrpcrt4 -Wl,--end-group/' $(BLD)/Makefile.Release
	@echo "[BUILD] Compiling (8 cores)..."
	$(MAKE) -C $(BLD) -j8
	cp $(BLD)/release/$(TARGET) ./$(TARGET)
	@echo "[OK] $(TARGET) — static single file, 0 DLLs needed"
	@ls -lh $(TARGET)

clean:
	rm -rf $(BLD) $(TARGET)
