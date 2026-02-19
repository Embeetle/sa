
extern unsigned __reset_vector;

void init_isr() {}

int main()
{
  return (int)&__reset_vector;
}
