Remove-Item .\game.exe -ErrorAction Ignore

$zip_file = '..\..\GoogleDrive\Code\c_game.zip'

Remove-Item $zip_file -ErrorAction Ignore

Compress-Archive ..\c_game\ $zip_file -Update