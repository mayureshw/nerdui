CXXFLAGS=	-I/usr/pkg/include -O3 -std=c++20
LDFLAGS=	-L/usr/pkg/lib -Wl,-rpath=/usr/pkg/lib -lyaml-cpp

yaml2h:	yaml2h.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -o $@
