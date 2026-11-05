-- timemachineplus schema init script
-- 幂等：所有表用 IF NOT EXISTS，可重复执行不破坏已有数据
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS tb_backuproot (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  rootpath TEXT
);

CREATE TABLE IF NOT EXISTS tb_backuptargetroot (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  targetrootpath TEXT
);

CREATE TABLE IF NOT EXISTS tb_backfiles (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  backuprootid INTEGER,
  filepath TEXT,
  versionhistorycnt INTEGER,
  lastbackuptime TEXT
);

CREATE TABLE IF NOT EXISTS tb_backfilehistory (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  backupfileid INTEGER,
  motifytime INTEGER,
  filesize INTEGER,
  copystarttime TEXT,
  copyendtime TEXT,
  backuptargetpath TEXT,
  backuptargetrootid INTEGER,
  md5 TEXT,
  backupid INTEGER
);

CREATE TABLE IF NOT EXISTS tb_backup (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  begintime TEXT,
  endtime TEXT,
  filecopycount INTEGER,
  datacopycount INTEGER
);
