FreeRTOS example stub. The kernel is vendored as a submodule:

```bash
git submodule update --init examples/FreeRTOS/FreeRTOS-Kernel
make example EX=FreeRTOS
./build/example_FreeRTOS
```
