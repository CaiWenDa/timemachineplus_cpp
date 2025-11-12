/*
 Navicat Premium Data Transfer

 Source Server         : 192.168.0.223
 Source Server Type    : MySQL
 Source Server Version : 50717
 Source Host           : 192.168.0.223:3306
 Source Schema         : timemachine

 Target Server Type    : MySQL
 Target Server Version : 50717
 File Encoding         : 65001

 Date: 01/07/2023 10:58:06
*/

-- Disable foreign key checks (SQLite does not support this directly)
PRAGMA foreign_keys = OFF;

-- ----------------------------
-- Table structure for tb_backfilehistory
-- ----------------------------
DROP TABLE IF EXISTS tb_backfilehistory;
CREATE TABLE tb_backfilehistory (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  backupfileid INTEGER,
  motifytime INTEGER, -- 文件修改时间
  filesize INTEGER, -- 文件大小
  copystarttime TEXT, -- 备份拷贝开始时间
  copyendtime TEXT, -- 备份拷贝结束时间
  backuptargetpath TEXT, -- 备份目标路径
  backuptargetrootid INTEGER, -- 备份目标id
  md5 TEXT,
  backupid INTEGER
);

-- ----------------------------
-- Table structure for tb_backfiles
-- ----------------------------
DROP TABLE IF EXISTS tb_backfiles;
CREATE TABLE tb_backfiles (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  backuprootid INTEGER, -- 来源文件夹表id
  filepath TEXT, -- 来源完整路径
  versionhistorycnt INTEGER, -- 备份次数
  lastbackuptime TEXT -- 最新备份时间
);

-- ----------------------------
-- Table structure for tb_backup
-- ----------------------------
DROP TABLE IF EXISTS tb_backup;
CREATE TABLE tb_backup (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  begintime TEXT, -- 备份开始时间
  endtime TEXT, -- 备份结束时间
  filecopycount INTEGER, -- 变动文件数
  datacopycount INTEGER -- 拷贝字节数
);

-- ----------------------------
-- Table structure for tb_backuproot
-- ----------------------------
DROP TABLE IF EXISTS tb_backuproot;
CREATE TABLE tb_backuproot (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  rootpath TEXT -- 来源根路径
);

-- ----------------------------
-- Table structure for tb_backuptargetroot
-- ----------------------------
DROP TABLE IF EXISTS tb_backuptargetroot;
CREATE TABLE tb_backuptargetroot (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  targetrootpath TEXT -- 备份目标路径根目录
);

-- Re-enable foreign key checks
PRAGMA foreign_keys = ON;
