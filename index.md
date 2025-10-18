# Get Started with JNTD
Welcome to JNTD! This guide will help you install, and prepare to use it!

## Quickstart: installation and configuration
The standard way of installing it is cloning the repo, with
```bash
git clone https://github.com/Lucasplaygaemes/JNTD
```
And then running the installer
```bash
./install.sh
```
But you can clone the repository and Try to just go and run Make, The dependencies are in requeriments.txt


# Commands
JNTD has a variety of built-in commands and supports plugins to extend its functionality.

## Main Commands

| Command | Description |
|---|---|
| `ls` | Diretorio atual e lista arquivos detalhadamente. |
| `:q` | Outro forma de sair do JNTD, criado pelo atalho do VIM. |
| `lsa` | Igual o listar, mas todos os arquivos (incluindo ocultos). |
| `data` | Data e hora atual. |
| `quem` | Nome do usuário atual. |
| `esp` | Espaço livre do sistema de arquivos atual. |
| `sysatt?` | Verifica se há atualizações do sistema disponíveis (Pop!_OS). |
| `sudo` | Entra no modo super usuário (USE COM CUIDADO!). |
| `help` | Lista todos os comandos disponíveis e suas descrições. |
| `criador` | Diz o nome do criador do JNTD e 2B. |
| `2b` | Inicia uma conversa com a 2B, e processa sua saida. |
| `log` | O codigo sempre salva um arquivo log para eventuais casualidades. |
| `his` | Exibe o histórico de comandos digitados. |
| `cl` | Limpa o terminal. |
| `git` | Mostra o link para Github do repositorio, além da ultima commit. |
| `mkdir` | Cria um novo diretorio. |
| `cp` | Copia arquivos e diretorios, use: cp <origem> <destino>. |
| `rm` | Remove arquivos ou diretorio, use rm <alvo>. |
| `mv` | Move o arquivos ou renomeia arquivos e diretorios, use: <origem> <destino>. |
| `rscript` | Roda um script pré definido, coloque cada comando em uma linha. |
| `sl` | Easter Egg. |
| `cd` | O comando cd, você troca de diretorio, use cd <destino>. |
| `pwd` | Fala o diretorio atual. |
| `vim` | Abre o editor, aceita nome para editar um arquivo. |
| `quiz` | Mostra todas as perguntas do quiz do integrado. |
| `quizt` | Define o intervalo de tempo entre os QUIZ'es. |
| `quizale` | Uma pergunta aleatoria do QUIZ é feita. |
| `timer` | Um simples timer. |
| `cp_di` | Um copy, use com <de qual arquivo para qual>. |
| `alias` | Adiciona alias. |
| `a2` | Inicia a A2, um editor de texto simples do JNTD. |
| `download` | Uma função de download, <use com download, depois irá pedir o nome do arquivo.>. |
| `buscar` | Uma função para buscar coisas pelo JNTD. |
| `elinks` | Elinks é um código que te permite fazer pesquisas na internet sem sair do terminal. |
| `awrit` | Awrit é um codigo que te permite usar o Chorimium pelo TERMINAL!. |
| `hash` | Verifica ou gera hashes SHA-256 para arquivos. |

## Plugin Commands

### `todo`
Gerencia tarefas.
- `todo add <tarefa>`: Adiciona uma nova tarefa.
- `todo list`: Lista todas as tarefas.
- `todo remove <numero>`: Remove uma tarefa pelo número.
- `todo edit <numero>`: Edita uma tarefa (não implementado).
- `todo check`: Verifica tarefas vencidas.
- `todo vim`: Abre o arquivo de tarefas no Vim.

### `calc`
Calculadora com funções básicas e avançadas.
- `calc soma <num1> <num2>`: Soma dois números.
- `calc sub <num1> <num2>`: Subtrai dois números.
- `calc mult <num1> <num2>`: Multiplica dois números.
- `calc div <num1> <num2>`: Divide dois números.
- `calc deriv_poly <c1> <e1> ...`: Derivada de polinômio.
- `calc deriv_trig <c> <tipo>`: Derivada de função trigonométrica (sin, cos, tan).
- `calc limit <a> <func>`: Limite de uma função em x=a.
- `calc integ <a> <b> <func>`: Integral de uma função de a a b.

### `example`
Plugin de exemplo.
- `example [args]`: Imprime uma mensagem de exemplo e os argumentos recebidos.
