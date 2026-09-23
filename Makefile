# --- Werkzeuge und Optionen ---
A68C     = ga68
A68FLAGS = -std=gnu68 -O2 -fstropping=upper
CC       = gcc
CFLAGS   = -O2 -pthread

# --- Projekt-Struktur ---
TARGET   = srv68
# Das Hauptprogramm (wird über die Musterregel aus srv68.u68 erzeugt)
MAIN     = srv68.a68
WRAPPER  = sock_wrapper.c
THREAD   = thread_wrapper.c
SQLITE   = sqlite_wrapper.c
WRAP_OBJ = sock_wrapper.o
THREAD_O = thread_wrapper.o
SQLITE_O = sqlite_wrapper.o

all: $(TARGET)

# Allgemeine Musterregel: Wandelt JEDE .u68 in eine .a68 um
%.a68: %.u68
	u682a68 < $< > $@
#	../../algol68g/a68g ../doc68/boldtoupperstrop.A68 < $< > $@

# Die C-Wrapper in .o-Objekte kompilieren
$(WRAP_OBJ): $(WRAPPER)
	$(CC) $(CFLAGS) -c $(WRAPPER) -o $(WRAP_OBJ)

$(THREAD_O): $(THREAD)
	$(CC) $(CFLAGS) -c $(THREAD) -o $(THREAD_O)

$(SQLITE_O): $(SQLITE)
	$(CC) $(CFLAGS) -c $(SQLITE) -o $(SQLITE_O)

# Das Algol-Hauptprogramm zusammen mit den C-Objekten linken
$(TARGET): $(MAIN) $(WRAP_OBJ) $(THREAD_O) $(SQLITE_O)
	$(A68C) $(A68FLAGS) $(MAIN) $(WRAP_OBJ) $(THREAD_O) $(SQLITE_O) -lsqlite3 -pthread -o $(TARGET)

.PHONY: all clean run

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(WRAP_OBJ) $(THREAD_O) $(SQLITE_O) *.a68

