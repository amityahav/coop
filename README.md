# coop

I was curious how user-space threads library could be implemented so i built a minimalistic library that does so. i was inspired by [this](https://brennan.io/2020/05/24/userspace-cooperative-multitasking/) great blog post that helped me to get the idea of one of the ways this can be achieved.
this is an educational project of course and not practical for production usage.
### Further improvements (that i wish to do in the future):

- Non-blocking IO:
  - [x] write()
  - [x] read()
  - [x] open()
  - [x] close()
- Preemptive scheduling

### Example: 
```c
void coop3(void *args) {
    int fd = coop_open("example.txt", O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    coop_print("coop3: reading from file\n");

    char res[5];
    coop_read(fd, res, 5);
    coop_close(fd);

    coop_print(res);
}

void coop2(void *args) {
    coop(coop3, NULL);

    int fd = coop_open("example.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    coop_print("coop2: writing to file\n");

    const char* buf = "Hello";
    coop_write(fd, buf, 5);

    coop_close(fd);
}

void coop1(void* args) {
    coop(coop2, NULL);

    for (int i = 0; i < 3; i++) {
        coop_print("coop1: Hey\n");
    }
}

int main(int argc, char**argv) {
    coop(coop1, NULL);
}

// Output:
//  coop1: Hey
//  coop1: Hey
//  coop2: writing to file
//  coop3: reading from file
//  coop1: Hey
//  Hello
```
