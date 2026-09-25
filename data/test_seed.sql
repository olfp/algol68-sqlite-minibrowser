PRAGMA foreign_keys = ON;

DROP TABLE IF EXISTS OrderDetails;
DROP TABLE IF EXISTS Orders;
DROP TABLE IF EXISTS Products;
DROP TABLE IF EXISTS Categories;
DROP TABLE IF EXISTS "Order Status";
DROP TABLE IF EXISTS Users;

CREATE TABLE Users (
  Id        INTEGER PRIMARY KEY,
  Name      TEXT NOT NULL,
  Email     TEXT NOT NULL UNIQUE,
  Active    INTEGER NOT NULL DEFAULT 1,
  ManagerId INTEGER REFERENCES Users(Id)
);

CREATE TABLE Categories (
  Id          INTEGER PRIMARY KEY,
  Name        TEXT NOT NULL,
  Description TEXT
);

CREATE TABLE Products (
  Id         INTEGER PRIMARY KEY,
  Name       TEXT NOT NULL,
  Price      REAL NOT NULL,
  InStock    INTEGER NOT NULL DEFAULT 0,
  CategoryId INTEGER REFERENCES Categories(Id)
);

CREATE TABLE "Order Status" (
  Code  TEXT PRIMARY KEY,
  Label TEXT NOT NULL
);

CREATE TABLE Orders (
  Id        INTEGER PRIMARY KEY,
  UserId    INTEGER NOT NULL REFERENCES Users(Id),
  Status    TEXT NOT NULL REFERENCES "Order Status"(Code),
  CreatedOn TEXT NOT NULL
);

CREATE TABLE OrderDetails (
  OrderId   INTEGER REFERENCES Orders(Id),
  ProductId INTEGER REFERENCES Products(Id),
  Quantity  INTEGER NOT NULL,
  UnitPrice REAL NOT NULL,
  PRIMARY KEY (OrderId, ProductId)
);

-- ---------------------------------------------------------------- Users
INSERT INTO Users (Id, Name, Email, Active, ManagerId) VALUES
  (1,  'Ada Lovelace',     'ada@example.com',      1, NULL),
  (2,  'Alan Turing',      'alan@example.com',     1, NULL),
  (3,  'Grace Hopper',     'grace@example.com',    1, 1),
  (4,  'Katherine Johnson','katherine@example.com',1, 1),
  (5,  'Edsger Dijkstra',  'edsger@example.com',   1, 2),
  (6,  'Barbara Liskov',   'barbara@example.com',  1, 2),
  (7,  'José Silva',       'jose@example.com',     1, 3),
  (8,  'Anja Müller',      'anja@example.com',     0, 3),
  (9,  'François Legrand', 'francois@example.com', 1, 4),
  (10, 'Hélène Dubois',    'helene@example.com',   0, 4),
  (11, 'Dmitri Volkov',    'dmitri@example.com',   1, 5),
  (12, 'Ingrid Andersen',  'ingrid@example.com',   1, 6),
  (13, 'Tomás Rivera',     'tomas@example.com',    1, 7),
  (14, 'Lars Johansson',   'lars@example.com',     0, 7),
  (15, 'Nadja Petrov',     'nadja@example.com',    1, 9);

-- ------------------------------------------------------------- Categories
INSERT INTO Categories (Id, Name, Description) VALUES
  (1,  'Electronics',     'Gadgets und Zubehoer'),
  (2,  'Clothing',        'Bekleidung fuer jede Jahreszeit'),
  (3,  'Books',           'Belletristik und Fachliteratur'),
  (4,  'Kitchen',         'Kuechengeraete und Utensilien'),
  (5,  'Sports',          'Sportgeraete und -kleidung'),
  (6,  'Toys',            'Spielzeug fuer Kinder und Erwachsene'),
  (7,  'Garden',          'Garten- und Balkonzubehoer'),
  (8,  'Health',          'Gesundheit und Pflege'),
  (9,  'Stationery',      'Buero- und Papierwaren'),
  (10, 'Music & Film',    'Tontraeger und Filme');

-- --------------------------------------------------------------- Products
INSERT INTO Products (Id, Name, Price, InStock, CategoryId) VALUES
  (1,  'Kopfhoerer X200',       89.99,  24, 1),
  (2,  'Kabellose Maus',        24.50,  60, 1),
  (3,  'Mechanical Keyboard',  119.00,  15, 1),
  (4,  'USB-C Hub',             39.90,  42, 1),
  (5,  'T-Shirt basic',         12.80, 120, 2),
  (6,  'Jeans slim',            49.90,  40, 2),
  (7,  'Regenjacke',            74.50,  18, 2),
  (8,  'Algol 68 in der Praxis',39.95,  8,  3),
  (9,  'SQLite Kochbuch',       45.00,  5,  3),
  (10, 'Krimi: Letzter Call',   14.90, 33, 3),
  (11, 'Espressokanne',         29.90, 25, 4),
  (12, 'Pfannenset',            59.90, 12, 4),
  (13, 'Messerset',             99.00,  9, 4),
  (14, 'Handtuch Yoga',         19.50, 55, 5),
  (15, 'Springseil',            11.90, 70, 5),
  (16, 'Fitnessband',           15.00, 48, 5),
  (17, 'Holzbausteine 100er',   29.90, 30, 6),
  (18, 'Puzzle 1000 Teile',     16.50, 22, 6),
  (19, 'Modellbausatz Segelboot',34.90, 14, 6),
  (20, 'Blumenkasten XL',       22.90, 36, 7),
  (21, 'Bewaesserungsset',      41.50, 11, 7),
  (22, 'Gartenhandschuhe',       9.90, 80, 7),
  (23, 'Vitamine C 1000',       12.50, 66, 8),
  (24, 'Aromatherapieset',      27.90, 17, 8),
  (25, 'Notizbuch A5',          4.90, 150, 9),
  (26, 'Farbstifte 36er',       11.50, 45, 9),
  (27, 'Ordner rot',            3.30, 200, 9),
  (28, 'Vinyl: The Best of...', 24.90,  6, 10),
  (29, 'Blu-ray Box',           39.90,  4, 10),
  (30, 'CD Compilation',        12.90, 13, 10);

-- -------------------------------------------------------------- Status
INSERT INTO "Order Status" (Code, Label) VALUES
  ('NEW',      'Neu'),
  ('PAID',     'Bezahlt'),
  ('SHIPPED',  'Versendet'),
  ('DONE',     'Abgeschlossen'),
  ('CANCELLED','Storniert');

-- ---------------------------------------------------------------- Orders
INSERT INTO Orders (Id, UserId, Status, CreatedOn) VALUES
  (1,  1,  'PAID',     '2026-08-02'),
  (2,  3,  'SHIPPED',  '2026-08-03'),
  (3,  5,  'DONE',     '2026-08-05'),
  (4,  7,  'DONE',     '2026-08-06'),
  (5,  9,  'NEW',      '2026-08-07'),
  (6,  11, 'PAID',     '2026-08-08'),
  (7,  13, 'SHIPPED',  '2026-08-09'),
  (8,  2,  'DONE',     '2026-08-10'),
  (9,  4,  'DONE',     '2026-08-11'),
  (10, 6,  'CANCELLED','2026-08-12'),
  (11, 8,  'DONE',     '2026-08-13'),
  (12, 10, 'PAID',     '2026-08-14'),
  (13, 12, 'DONE',     '2026-08-15'),
  (14, 14, 'NEW',      '2026-08-16'),
  (15, 15, 'SHIPPED',  '2026-08-17'),
  (16, 1,  'SHIPPED',  '2026-08-18'),
  (17, 3,  'PAID',     '2026-08-19'),
  (18, 5,  'DONE',     '2026-08-20'),
  (19, 7,  'CANCELLED','2026-08-21'),
  (20, 9,  'DONE',     '2026-08-22'),
  (21, 11, 'PAID',     '2026-08-23'),
  (22, 13, 'NEW',      '2026-08-24'),
  (23, 2,  'SHIPPED',  '2026-08-25'),
  (24, 4,  'DONE',     '2026-08-26'),
  (25, 6,  'PAID',     '2026-08-27'),
  (26, 8,  'SHIPPED',  '2026-08-28'),
  (27, 10, 'DONE',     '2026-08-29'),
  (28, 12, 'DONE',     '2026-08-30'),
  (29, 14, 'SHIPPED',  '2026-08-31'),
  (30, 15, 'PAID',     '2026-09-01'),
  (31, 1,  'PAID',     '2026-09-02'),
  (32, 3,  'DONE',     '2026-09-03'),
  (33, 5,  'SHIPPED',  '2026-09-04'),
  (34, 7,  'PAID',     '2026-09-05'),
  (35, 9,  'NEW',      '2026-09-06'),
  (36, 11, 'DONE',     '2026-09-07'),
  (37, 13, 'PAID',     '2026-09-08'),
  (38, 2,  'SHIPPED',  '2026-09-09'),
  (39, 4,  'DONE',     '2026-09-10'),
  (40, 6,  'PAID',     '2026-09-11'),
  (41, 8,  'NEW',      '2026-09-12'),
  (42, 10, 'SHIPPED',  '2026-09-13'),
  (43, 12, 'DONE',     '2026-09-14'),
  (44, 14, 'PAID',     '2026-09-15'),
  (45, 15, 'SHIPPED',  '2026-09-16'),
  (46, 1,  'DONE',     '2026-09-17'),
  (47, 3,  'NEW',      '2026-09-18'),
  (48, 5,  'PAID',     '2026-09-19'),
  (49, 7,  'SHIPPED',  '2026-09-20'),
  (50, 9,  'PAID',     '2026-09-21');

-- -------------------------------------------------------------- Details
INSERT INTO OrderDetails (OrderId, ProductId, Quantity, UnitPrice) VALUES
  (1, 1, 1, 89.99),  (1, 4, 1, 39.90),
  (2, 6, 2, 49.90),  (2, 7, 1, 74.50),
  (3, 8, 2, 39.95),
  (4, 11, 1, 29.90), (4, 12, 1, 59.90),
  (5, 15, 3, 11.90),
  (6, 3, 1, 119.00),
  (7, 18, 2, 16.50), (7, 17, 1, 29.90),
  (8, 14, 2, 19.50),
  (9, 25, 5, 4.90),  (9, 26, 2, 11.50),
  (10, 2, 2, 24.50),
  (11, 20, 3, 22.90), (11, 22, 2, 9.90),
  (12, 28, 2, 24.90),
  (13, 11, 1, 29.90), (13, 13, 1, 99.00),
  (14, 5, 2, 12.80),
  (15, 24, 2, 27.90),
  (16, 9, 1, 45.00),
  (17, 1, 2, 89.99), (17, 3, 1, 119.00),
  (18, 27, 10, 3.30),
  (19, 7, 1, 74.50),
  (20, 21, 1, 41.50), (20, 22, 1, 9.90),
  (21, 16, 4, 15.00),
  (22, 19, 1, 34.90),
  (23, 2, 3, 24.50),
  (24, 25, 8, 4.90),
  (25, 12, 1, 59.90), (25, 11, 1, 29.90),
  (26, 4, 2, 39.90),
  (27, 9, 1, 45.00), (27, 10, 2, 14.90),
  (28, 13, 1, 99.00),
  (29, 6, 1, 49.90),
  (30, 28, 1, 24.90), (30, 29, 1, 39.90),
  (31, 1, 1, 89.99), (31, 2, 1, 24.50),
  (32, 23, 3, 12.50),
  (33, 15, 2, 11.90), (33, 16, 2, 15.00),
  (34, 17, 1, 29.90),
  (35, 20, 2, 22.90),
  (36, 8, 1, 39.95), (36, 9, 1, 45.00),
  (37, 3, 2, 119.00), (37, 4, 1, 39.90),
  (38, 14, 1, 19.50),
  (39, 25, 6, 4.90), (39, 27, 4, 3.30),
  (40, 11, 2, 29.90),
  (41, 5, 3, 12.80),
  (42, 21, 1, 41.50),
  (43, 2, 2, 24.50), (43, 3, 1, 119.00),
  (44, 18, 1, 16.50),
  (45, 24, 3, 27.90), (45, 23, 2, 12.50),
  (46, 10, 2, 14.90),
  (47, 13, 1, 99.00),
  (48, 6, 2, 49.90), (48, 7, 1, 74.50),
  (49, 1, 1, 89.99),
  (50, 30, 2, 12.90), (50, 28, 1, 24.90);