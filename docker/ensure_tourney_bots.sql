-- Official create only (Login Server CmdCreateUser):
--   pangya.ProcNewUser(id, plaintext_password, ip, server_uid)
-- FIRST_LOGIN / FIRST_SET stay 0. The IOCP bot does the official
-- first-login setup (client 0x07/0x06 nick, then 0x08 character).
--
-- Bots: test1..test4 / password 123456
SET NOCOUNT ON;

DECLARE @id VARCHAR(22);
DECLARE @uid INT;
DECLARE @i INT = 1;
DECLARE @pass_plain VARCHAR(32) = N'123456';

WHILE @i <= 4
BEGIN
	SET @id = CONCAT(N'test', CAST(@i AS VARCHAR(4)));
	SET @uid = NULL;
	SELECT @uid = UID FROM pangya.account WHERE ID = @id;

	IF @uid IS NULL
	BEGIN
		EXEC pangya.ProcNewUser @id, @pass_plain, N'127.0.0.1', 0;
		SELECT @uid = UID FROM pangya.account WHERE ID = @id;
	END

	IF @uid IS NULL
	BEGIN
		RAISERROR(N'ProcNewUser did not create %s', 16, 1, @id);
		RETURN;
	END

	UPDATE pangya.account
		SET PASSWORD = CONVERT(varchar(32), HASHBYTES(N'md5', @pass_plain), 2),
			Logon = 0,
			IDState = 0
		WHERE UID = @uid;

	IF NOT EXISTS (SELECT 1 FROM pangya.pangya_player_ip WHERE [uid] = @uid)
		INSERT INTO pangya.pangya_player_ip([uid], [ip]) VALUES (@uid, N'127.0.0.1');

	SET @i = @i + 1;
END

SELECT UID, ID, NICK, FIRST_SET, FIRST_LOGIN, Logon
	FROM pangya.account
	WHERE ID IN (N'test1', N'test2', N'test3', N'test4')
	ORDER BY ID;
