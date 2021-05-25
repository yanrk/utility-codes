for %%a in ("E:\test\test.exe") do (
    echo File Name Only       : %%~na
    echo File Extension       : %%~xa
    echo Name in 8.3 notation : %%~sna
    echo File Attributes      : %%~aa
    echo Located on Drive     : %%~da
    echo File Size            : %%~za
    echo Last-Modified Date   : %%~ta
    echo Drive and Path       : %%~dpa
    echo Drive                : %%~da
    echo Fully Qualified Path : %%~fa
    echo FQP in 8.3 notation  : %%~sfa
    echo Location in the PATH : %%~dp$PATH:a
)
