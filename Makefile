PLATFORM := posix
MP4V2_SRC := $(wildcard mp4v2/src/*.cpp)
MP4V2_SRC += $(wildcard mp4v2/src/bmff/*.cpp)
MP4V2_SRC += $(wildcard mp4v2/src/itmf/*.cpp)
MP4V2_SRC += $(wildcard mp4v2/src/qtff/*.cpp)
MP4V2_SRC += mp4v2/libplatform/io/File.cpp
MP4V2_SRC += mp4v2/libplatform/io/File_$(PLATFORM).cpp
MP4V2_SRC += mp4v2/libplatform/io/FileSystem.cpp
MP4V2_SRC += mp4v2/libplatform/io/FileSystem_$(PLATFORM).cpp
MP4V2_SRC += mp4v2/libplatform/number/random_$(PLATFORM).cpp
MP4V2_SRC += mp4v2/libplatform/process/process_$(PLATFORM).cpp
MP4V2_SRC += mp4v2/libplatform/sys/error.cpp
MP4V2_SRC += mp4v2/libplatform/time/time.cpp
MP4V2_SRC += mp4v2/libplatform/time/time_$(PLATFORM).cpp
ifeq ($(PLATFORM),win32)
    MP4V2_SRC += mp4v2/libplatform/platform_win32.cpp
endif

SRC := $(wildcard src/*.cpp)

OBJS := $(patsubst %.cpp,%.o,$(SRC) $(MP4V2_SRC))

CFLAGS := -O2 -Wno-literal-suffix

mp4vttextract: $(OBJS)
	$(CXX) -o $@ $(OBJS)

%.o : %.cpp
	$(CXX) -c $(CFLAGS) $(CPPFLAGS) -Imp4v2 -Imp4v2/include -o $@ $<

clean:
	find . -name '*.o' -exec rm -f {} +
