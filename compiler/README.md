## Command to run the JOCKY pipeline to get onfuscated .exe

1. Change to jocky directory
    `cd compiler\jocky\driver`
2. Command
    `.\jocky.exe \path\to\your\file.cpp -o \path\to\your\result.exe "-passes=fla,sub,bcf,gvenc,mba,indcall,indbr" -v`

Example-
`.\compiler\jocky\driver\jocky.exe C:\Users\Ameya\Documents\GitHub\JOCKY-TSAR\build\evil.cpp -o test_output.exe "-passes=fla,sub,bcf,gvenc,mba,indcall,indbr" -v`