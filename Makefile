# --- Werkzeuge und Optionen ---
A68C     = ga68
A68FLAGS = -std=gnu68 -O2 -fstropping=upper
CC       = gcc
CFLAGS   = -O2 -pthread

# --- Projekt-Struktur ---
# Es wird ausschliesslich im separaten build/-Ordner gebaut; die Quellen
# (.u68, .c, u682a68) bleiben unveraendert im Repo. ga68 legt beim
# Kompilieren ("-c") das Objekt im aktuellen Verzeichnis ab, deshalb wird
# fuer Compile und Link in den Build-Ordner gewechselt.
BUILD_DIR = build/run
TARGET    = gorgona
BIN       = $(BUILD_DIR)/$(TARGET)

# Das Hauptprogramm (Particular Program mit ACCESS-Klausel; wird ueber die
# Musterregel aus gorgona.u68 erzeugt) - alles unter BUILD_DIR.
MAIN    = $(BUILD_DIR)/gorgona.a68
# Die Definition-Module in der Reihenfolge fuer den Link (transio zuerst,
# dann strings, dann url: url greift per ACCESS STRINGS auf strings zu.
# Jedes Modul wird separat mit "ga68 -c" gebaut; der Objektname entspricht
# dann automatisch dem lowercase Modul-Indikant: transio.o, strings.o,
# url.o - so findet das ACCESS die Exports wieder.
MODULES = $(BUILD_DIR)/transio.a68 $(BUILD_DIR)/strings.a68 $(BUILD_DIR)/url.a68
MOD_OBJS= $(BUILD_DIR)/transio.o $(BUILD_DIR)/strings.o $(BUILD_DIR)/url.o
MAIN_O  = $(BUILD_DIR)/gorgona.o

# Der minimale C-Kern fuer byte-exakte Ein-/Ausgabe (roh lesen/schreiben)
TRANS    = transput_wrapper.c
WRAPPER  = sock_wrapper.c
THREAD   = thread_wrapper.c
SQLITE   = sqlite_wrapper.c
TRANS_O  = $(BUILD_DIR)/transput_wrapper.o
WRAP_OBJ = $(BUILD_DIR)/sock_wrapper.o
THREAD_O = $(BUILD_DIR)/thread_wrapper.o
SQLITE_O = $(BUILD_DIR)/sqlite_wrapper.o

all: $(BIN)

# Den Build-Ordner anlegen, sobald er benoetigt wird
$(BUILD_DIR):
	mkdir -p $@

# Die u682a68-Ausgaben nicht als Make-Intermediates automatisch loeschen
.SECONDARY: $(MAIN) $(MODULES)

# Musterregel: Wandelt JEDE .u68 aus dem Repo in eine .a68 unter
# BUILD_DIR um (u682a68 liest vom Repo-Quellpfad, schreibt in den Build-Ordner)
$(BUILD_DIR)/%.a68: %.u68 | $(BUILD_DIR)
	python3 u682a68 < $< > $@

# Jedes Algol-Modul separat kompilieren: ga68 legt die gleichnamige .o
# im aktuellen Verzeichnis ab, deshalb wird in den Build-Ordner gewechselt
$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.a68 | $(BUILD_DIR)
	cd $(BUILD_DIR) && $(A68C) $(A68FLAGS) -c $(notdir $<)

# Das Hauptprogramm braucht beim Kompilieren die Exports der Module,
# deshalb haengt gorgona.o an den Modul-Objekten
$(MAIN_O): $(MAIN) $(MOD_OBJS)

# Die C-Wrapper in .o-Objekte kompilieren
$(TRANS_O): $(TRANS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(TRANS) -o $@

$(WRAP_OBJ): $(WRAPPER) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(WRAPPER) -o $@

$(THREAD_O): $(THREAD) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(THREAD) -o $@

$(SQLITE_O): $(SQLITE) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(SQLITE) -o $@

# Das Algol-Hauptprogramm zusammen mit den Modulen und C-Objekten linken
$(BIN): $(MAIN_O) $(MOD_OBJS) $(TRANS_O) $(WRAP_OBJ) $(THREAD_O) $(SQLITE_O)
	cd $(BUILD_DIR) && $(A68C) $(A68FLAGS) $(notdir $(MAIN_O)) $(notdir $(MOD_OBJS)) $(notdir $(TRANS_O)) $(notdir $(WRAP_OBJ)) $(notdir $(THREAD_O)) $(notdir $(SQLITE_O)) -lsqlite3 -pthread -o $(TARGET)

.PHONY: all run clean

run: $(BIN)
	./$(BIN)

clean:
	rm -rf build