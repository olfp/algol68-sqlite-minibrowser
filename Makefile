# --- Werkzeuge und Optionen ---
A68C     = ga68
A68FLAGS = -std=gnu68 -O2 -fstropping=upper
CC       = gcc
CFLAGS   = -O2 -pthread

# --- Projekt-Struktur ---
TARGET   = gorgona
# Das Hauptprogramm (wird über die Musterregel aus gorgona.u68 erzeugt)
MAIN     = gorgona.a68
# Das Unicode-Transput-Modul (Deklarationen, wird hinter das BEGIN
# des Hauptprogramms eingefügt, damit der Stock-ga68 keine Modul-Units
# braucht) - und der minimale C-Kern dafür (roh lesen/schreiben, byte-exakt)
TRANSMOD = transput.u68
TRANSMOD_A = transput.a68
JOINED   = gorgona-joined.a68
TRANS    = transput_wrapper.c
WRAPPER  = sock_wrapper.c
THREAD   = thread_wrapper.c
SQLITE   = sqlite_wrapper.c
TRANS_O  = transput_wrapper.o
WRAP_OBJ = sock_wrapper.o
THREAD_O = thread_wrapper.o
SQLITE_O = sqlite_wrapper.o

all: $(TARGET)

# Allgemeine Musterregel: Wandelt JEDE .u68 in eine .a68 um
%.a68: %.u68
	u682a68 < $< > $@

# Das Transput-Modul hinter das öffnende BEGIN des Hauptprogramms schieben:
# Zeile 1 des Hauptprogramms ist "BEGIN", direkt danach folgen die
# Modul-Deklarationen, damit sie vor allen Verwendungen gelten.
$(JOINED): $(MAIN) $(TRANSMOD_A)
	awk 'NR==1 && /^BEGIN$$/ {print; while ((getline line < "$(TRANSMOD_A)") > 0) print line; close("$(TRANSMOD_A)"); next} {print}' $(MAIN) > $@

# Die C-Wrapper in .o-Objekte kompilieren
$(TRANS_O): $(TRANS)
	$(CC) $(CFLAGS) -c $(TRANS) -o $(TRANS_O)

$(WRAP_OBJ): $(WRAPPER)
	$(CC) $(CFLAGS) -c $(WRAPPER) -o $(WRAP_OBJ)

$(THREAD_O): $(THREAD)
	$(CC) $(CFLAGS) -c $(THREAD) -o $(THREAD_O)

$(SQLITE_O): $(SQLITE)
	$(CC) $(CFLAGS) -c $(SQLITE) -o $(SQLITE_O)

# Das Algol-Hauptprogramm zusammen mit den C-Objekten linken
$(TARGET): $(JOINED) $(TRANS_O) $(WRAP_OBJ) $(THREAD_O) $(SQLITE_O)
	$(A68C) $(A68FLAGS) $(JOINED) $(TRANS_O) $(WRAP_OBJ) $(THREAD_O) $(SQLITE_O) -lsqlite3 -pthread -o $(TARGET)

.PHONY: all clean run

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TRANS_O) $(WRAP_OBJ) $(THREAD_O) $(SQLITE_O) config_wrapper.o *.a68

