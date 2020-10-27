# apott Aaron Ott, blboss Brendan Boss
prob_2: prob_2.cpp
	g++ -g -O0 prob_2.cpp -o prob_2

outputter: outputter.cpp
	g++ -g -O0 outputter.cpp -o outputter

clean:
	rm -f prob_2
