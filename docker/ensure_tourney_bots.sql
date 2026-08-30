-- Idempotent Tourney bot accounts: test1..test4 / password 123456 (MD5 below).
-- Nickname equals login id so the IOCP bot can match oid from 0x48.
SET NOCOUNT ON;

DECLARE @pass VARCHAR(32) = N'e10adc3949ba59abbe56e057f20f883e'; -- MD5(123456)
DECLARE @id VARCHAR(22);
DECLARE @uid INT;
DECLARE @charId INT;
DECLARE @i INT = 1;

WHILE @i <= 4
BEGIN
	SET @id = CONCAT(N'test', CAST(@i AS VARCHAR(4)));
	SET @uid = NULL;
	SELECT @uid = UID FROM pangya.account WHERE ID = @id;

	IF @uid IS NULL
	BEGIN
		EXEC pangya.ProcNewUserWithMD5 @id, @pass, 0, N'127.0.0.1', 0;
		SELECT @uid = UID FROM pangya.account WHERE ID = @id;
	END

	UPDATE pangya.account
		SET PASSWORD = @pass,
			NICK = @id,
			FIRST_LOGIN = 1,
			FIRST_SET = 1,
			Logon = 0
		WHERE UID = @uid;

	IF NOT EXISTS (SELECT 1 FROM pangya.pangya_character_information WHERE UID = @uid)
	BEGIN
		EXEC pangya.ProcAddCharacter @uid, -1, 67108875, 0, 0, 1, 0,
			137102336, 137110528, 137118720, 137126912, 137135104, 137143296, 137151488, 137159680,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;
	END

	IF NOT EXISTS (
		SELECT 1 FROM pangya.pangya_item_warehouse
		WHERE UID = @uid AND typeid = 268435456
	)
		EXEC pangya.ProcFirstSet @uid;

	SELECT TOP 1 @charId = item_id
		FROM pangya.pangya_character_information
		WHERE UID = @uid
		ORDER BY item_id;

	IF @charId IS NOT NULL
		UPDATE pangya.pangya_user_equip
			SET character_id = @charId
			WHERE UID = @uid AND (character_id IS NULL OR character_id = 0);

	SET @i = @i + 1;
END

SELECT UID, ID, NICK, FIRST_SET, FIRST_LOGIN
	FROM pangya.account
	WHERE ID IN (N'test1', N'test2', N'test3', N'test4')
	ORDER BY ID;
