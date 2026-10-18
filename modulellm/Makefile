CXX ?= g++
CPPFLAGS += -I../src -I../lib/AvatarMicFFT/src
CXXFLAGS ?= -O2 -std=c++11 -Wall -Wextra -Werror

modulellm-vowel: main.cpp ModelStore.h VowelEngine.h ../src/VowelWire.h ../src/VowelAnalyzer.h ../src/LpcFormants.h ../src/VowelModel.h ../src/VowelCalibration.h ../src/MicFftAnalyzer.h ../lib/AvatarMicFFT/src/fft.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) main.cpp -o $@

clean:
	rm -f modulellm-vowel

.PHONY: clean
