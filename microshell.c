#include <unistd.h>
#include <stdlib.h>
#include <string.h>

# define R_END	0
# define W_END	1

typedef struct s_cmd {
    char** args;
    struct s_cmd* pipe;
    struct s_cmd* next;
} t_cmd;

int ft_strlen(char* s)
{
    int len = 0;
    while (s[len])
        len++;
    return (len);
}

char* ft_strdup(char* s)
{
    char* res = malloc(sizeof(char) * (ft_strlen(s) + 1));
    int i = -1;
    while (s[++i])
        res[i] = s[i];
    res[i] = '\0';
    return (res);
}

void free_cmds(t_cmd* cmds)
{
    t_cmd* it = cmds;
    while (it)
    {
        for (int i = 0; it->args[i]; i++)
            free(it->args[i]);
        free(it->args);
        t_cmd* tmp = it;
        if (it->pipe)
            it = it->pipe;
        else
            it = it->next;
        free(tmp);
    }
}

void error(char* msg, char* details)
{
    char* err = "error: ";
    write(STDERR_FILENO, err, ft_strlen(err));
    write(STDERR_FILENO, msg, ft_strlen(msg));
    if (details)
        write(STDERR_FILENO, details, ft_strlen(details));
    write(STDERR_FILENO, "\n", 1);
}

void fatal_error(t_cmd* cmds)
{
    free_cmds(cmds);
    error("fatal", NULL);
    exit(EXIT_FAILURE);
}

static int count_args(char** args)
{
    int n = 0;
    while (args[n] && strcmp(args[n], "|") && strcmp(args[n], ";"))
        n++;
    return (n);
}

static t_cmd* set_cmd()
{
    t_cmd* cmd = (t_cmd*)malloc(sizeof(t_cmd));
    cmd->pipe = NULL;
    cmd->next = NULL;
    cmd->args = NULL;
    return (cmd);
}

static int set_args(t_cmd** cmd, int args_size, char** args)
{
    if (!args_size)
        return (0);
    (*cmd)->args = malloc(sizeof(char*) * (args_size + 1));
    int i = -1;
    while (args[++i] && i < args_size)
        (*cmd)->args[i] = ft_strdup(args[i]);
    (*cmd)->args[i] = NULL;
    return (0);
}

t_cmd* parse_input(int n, char** args)
{
    t_cmd* cmds = set_cmd();
    t_cmd* it = cmds;
    for (int i = 0; i < n; ++i)
    {
        int count = count_args(&args[i]);
        set_args(&it, count, &args[i]);
        if ((i += count) >= n)
            break ;
        else if (strcmp(args[i], "|") == 0)
        {
            it->pipe = set_cmd();
            it = it->pipe;
        }
        else if (strcmp(args[i], ";") == 0)
        {
            it->next = set_cmd();
            it = it->next;
        }
    }
    return (cmds);
}

void builtin_cd(t_cmd* cmd)
{
    int i = 0;
    while (cmd->args[i])
        i++;
    if (i != 2)
        error("cd: bad arguments", NULL);
    else if (chdir(cmd->args[1]))
        error("cd: cannot change directory to ", cmd->args[1]);
}

static void redirect_pipe_end(int old, int new)
{
    if (old == new)
        return ;
    dup2(old, new);
    close(old);
}

int exec_command(t_cmd* cmd, char** env)
{
    int pfd[2], in, out, status;
    pid_t pid;

    in = STDIN_FILENO;
    while (cmd)
    {
        if (pipe(pfd) == -1)
            return (-1);
        if ((pid = fork()) == -1)
            return (-1);
        if (pid == 0)
        {
            close(pfd[R_END]);
            redirect_pipe_end(in, STDIN_FILENO);
            out = cmd->pipe ? pfd[W_END] : STDOUT_FILENO;
		    redirect_pipe_end(out, STDOUT_FILENO);
            if (execve(cmd->args[0], cmd->args, env) == -1)
                error("cannot execute ", cmd->args[0]);
        }
        close(pfd[W_END]);
        redirect_pipe_end(pfd[R_END], in);
        waitpid(pid, &status, 0);
        cmd = cmd->pipe;
    }
    close(in);
    return (0);
}

int main(int ac, char** av, char** env)
{
    if (ac == 1)
        return (0);
    t_cmd* cmds = parse_input(ac - 1, &av[1]);
    for (t_cmd* it = cmds; it; it = it->next)
    {
        if (strcmp(it->args[0], "cd") == 0)
            builtin_cd(it);
        else if (exec_command(it, env) < 0)
            fatal_error(cmds);
        while (it->pipe)
            it = it->pipe;
    }
    free_cmds(cmds);

    return (0);
}