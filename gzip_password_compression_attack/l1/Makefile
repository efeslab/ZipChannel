all: L1-rattle attacker targetted_attacker

%: %.c
	g++ -O2 $< -o $@ -ggdb

clean:
	rm -f L1-rattle
	rm -f attacker
	rm -f targetted_attacker
