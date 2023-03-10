for /l %%x in (1, 1, 5) do copy /Y challenge_loader_python.dll challenge_loader_python%%x.dll
timeout 3

::for /l %%x in (1, 1, 5) do (
::    if not exist challenge_loader_python.dll%%x.md (
::        copy challenge_loader_python.dll challenge_loader_python%%x.dll
::    )
::)