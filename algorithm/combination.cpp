void combination()
{
    int s = 6;
    int c = 4;
    std::vector<int> pos(c, 0);
    int i = 0;
    int k = 0;
    while (true)
    {
        if (k < c)
        {
            if (i < s)
            {
                pos[k++] = i++;
            }
            else if (k > 0)
            {
                i = pos[--k] + 1;
            }
            else
            {
                break;
            }
        }
        else
        {
        //  show it
        //  std::cout << pos[0] << ", " << pos[1] << ", " << pos[2] << ", " << pos[3] << std::endl;

            --k;

            if (s == i)
            {
                i = pos[--k] + 1;
            }
        }
    }
}
