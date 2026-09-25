# --- Werkzeuge und Optionen ---
A68C     = ga68
A68FLAGS = -std=gnu68 -O2 -fstropping=upper
CC       = gcc
CFLAGS   = -O2 -pthread

# --- Projekt-Struktur ---
TARGET   = gorgona
# Das Hauptprogramm (Particular Program mit ACCESS-Klausel; wird ueber die
# Musterregel aus gorgona.u68 erzeugt)
MAIN     = gorgona.a68
# Die Definition-Module in der Reihenfolge fuer den Link (transio zuerst,
# dann strings, dann url: url greift per ACCESS STRINGS auf strings zu.
# Jedes Modul wird separat mit "ga68 -c" gebaut; der Objektname entspricht
# dann automatisch dem lowercase Modul-Indikant: transio.o, strings.o,
# url.o - so findet das ACCESS die Exports wieder.
MODULES   = transio.a68 strings.a68 url.a68
# Der minimale C-Kern fuer byte-exakte Ein-/Ausgabe (roh lesen/schreiben)
TRANS     = transput_wrapper.c
WRAPPER   = sock_wrapper.c
THREAD    = thread_wrapper.c
SQLITE    = sqlite_wrapper.c
TRANS_O   = transput_wrapper.o
WRAP_OBJ  = sock_wrapper.o
THREAD_O  = thread_wrapper.o
SQLITE_O  = sqlite_wrapper.o

all: $(TARGET)

# Die u682a68-Ausgaben nicht als Make-Intermediates automatisch loeschen
.SECONDARY: $(MAIN) $(MODULES)

# Allgemeine Musterregel: Wandelt JEDE .u68 in eine .a68 um
%.a68: %.u68
	u682a68 < $< > $@

# Jedes Algol-Modul separat kompilieren (legt gleich <name>.o an)
%.o: %.a68
	$(A68C) $(A68FLAGS) -c $<

# Das Hauptprogramm braucht beim Kompilieren die Exports der Module,
# deshalb haengt gorgona.o an den Modul-Objekten
gorgona.o: $(MAIN) $(MODULES:.a68=.o)

# Die C-Wrapper in .o-Objekte kompilieren
$(TRANS_O): $(TRANS)
	$(CC) $(CFLAGS) -c $(TRANS) -o $(TRANS_O)

$(WRAP_OBJ): $(WRAPPER)
	$(CC) $(CFLAGS) -c $(WRAPPER) -o $(WRAP_OBJ)

$(THREAD_O): $(THREAD)
	$(CC) $(CFLAGS) -c $(THREAD) -o $(THREAD_O)

$(SQLITE_O): $(SQLITE)
	$(CC) $(CFLAGS) -c $(SQLITE) -o $(SQLITE_O)

# Das Algol-Hauptprogramm zusammen mit den Modulen und C-Objekten linken
$(TARGET): gorgona.o $(MODULES:.a68=.o) $(TRANS_O) $(WRAP_OBJ) $(THREAD_O) $(SQLITE_O)
	$(A68C) $(A68FLAGS) gorgona.o $(MODULES:.a68=.o) $(TRANS_O) $(WRAP_OBJ) $(THREAD_O) $(SQLITE_O) -lsqlite3 -pthread -o $(TARGET)

.PHONY: all clean run

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) gorgona.o $(MODULES:.a68=.o) $(TRANS_O) $(WRAP_OBJ) $(THREAD_O) $(SQLITE_O) *.a68