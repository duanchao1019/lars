USE lars_dns;

SET @time = UNIX_TIMESTAMP(NOW());

INSERT INTO RouteData(modid, cmdid, serverip, serverport) VALUES(1, 3, 3232235952, 8888);
UPDATE RouteVersion SET version = @time WHERE id = 1;

INSERT INTO RouteChange(modid, cmdid, version) VALUES(1, 1, @time);