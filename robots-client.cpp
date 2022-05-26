#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind/bind.hpp>
#include <boost/array.hpp>
#include <boost/program_options.hpp>
#include <vector>
#include <map>

// anonimowa przestrzeń nazw 
namespace { 


const size_t MAX_LENGTH = 65536; // maksymalna długość datagramu
const int64_t PORT_MAX = 65536; // maksymalny poprawny numer portu
const size_t LEN_OF_SIZETYPE = 4; 
// długość zapisu długości stringa, listy lub mapy
const size_t BYTE_SIZE = 256; // maksymalna wartość liczby jednobajtowej


typedef uint32_t BombId;
typedef uint8_t PlayerId;
typedef uint32_t Score;

/* Deserializuje liczbę o rozmiarze size. */
size_t deserialize_number(char data[], size_t size)
{
    size_t res = 0;
    for (size_t i = 0; i < size; i++)
    {
        res *= BYTE_SIZE;
        res += data[size - i - 1];       
    }
    return res;
}

/* Serializuje numer o rozmiarze size do tablicy data. */
void serialize_number(char data[], size_t number, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        data[size - i - 1] = number % BYTE_SIZE;
        number /= BYTE_SIZE;       
    }
}

/* Serializuje string s do tablicy data.
Zwracana wartość to długość całego zapisu stringa. */
size_t serialize_string(char data[], std::string s)
{
    size_t len = s.length();
    serialize_number(data, len, LEN_OF_SIZETYPE);
    size_t pos = LEN_OF_SIZETYPE;
    for (int i = 0; i < len; i++)
    {
        data[pos + i] = s[i];
        
    }
    return LEN_OF_SIZETYPE + len;
}

/* Deserializuje string do tablicy data,
w zmiennej pos zapisuje pozycję po nim. */
std::string deserialize_string(char data[], size_t& pos)
{
    size_t len = deserialize_number(data, LEN_OF_SIZETYPE);
    std::string s;
    for (int i = 0; i < len; i++)
    {
        s = s + char(data[i + LEN_OF_SIZETYPE]);
    }
    pos = LEN_OF_SIZETYPE + len;
    return s;
}


enum class GameStateType
{
    Lobby,
    Game
};

enum class Direction
{
    Up,
    Right,
    Down,
    Left
};

/* Serializuje kierunek. Zwraca długość zapisu. */
size_t serialize_direction(Direction direction, char data[])
{
    switch(direction)
    {
        case Direction::Up:
        {
            data[0] = 0;
            break;
        }
        case Direction::Right:
        {
            data[0] = 1;
            break;
        }
        case Direction::Down:
        {
            data[0] = 2;
            break;
        }
        case Direction::Left:
        {
            data[0] = 3;
            break;
        }
        default:
            exit(1);
    }
    return 1;
}

/* Deserializuje kierunek. */
Direction deserialize_direction(char data[])
{
    Direction d;
    switch(data[0])
    {
        case 0:
        {
            d = Direction::Up;
            break;
        }
        case 1:
        {
            d = Direction::Right;
            break;
        }
        case 2:
        {
            d = Direction::Down;
            break;
        }
        case 3:
        {
            d = Direction::Left;
            break;
        }
        default:
            exit(1);
    }
    return d;
    
}

class Position
{
    public:

    uint16_t x;
    uint16_t y;
};

class Player
{
    public:

    std::string name;
    std::string address;
    Position position;
};
/* Deserializuje pole player.
Do zmiennej len zapisuje długość zapisu. */
Player deserialize_player(char data[], size_t& len)
{
    Player p;
    size_t pos = 0;
    p.name = deserialize_string(data, pos);
    len = pos;
    p.address = deserialize_string(&(data[pos]), pos);
    len += pos;
    return p;
}


class Bomb
{
    public:

    BombId id;
    Position position;
    uint16_t timer;
};


/* Klasa służy do przechowywania aktualnych informacji o grze.
Jest tworzona gdy otrzymamy Hello. */
class Game 
{
    public:

    GameStateType game_state; // lobby lub game
    std::string server_name;
    uint8_t players_count;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t game_length;
    uint16_t explosion_radius;
    uint16_t bomb_timer;
    uint16_t turn;
    std::map<PlayerId, Player> players;
    std::map<PlayerId, Score> scores;
    std::vector<Position> blocks;
};

Game current_game; // tu przechowujemy informacje o obecnej grze


/* Po użyciu obiekt tej klasy jest usuwany. */
class Event
{
    public:
    Event() {}

    // zmienia stan gry zgodnie z otrzymanym eventem
    virtual void save_event_to_game(Game& game) {};
};

class BombPlaced : public Event
{
    public:

    BombId id;
    Position position;
    void save_event_to_game(Game& game)
    {
        /* Nic się nie dzieje -
        jako klient nie musimy pamiętać, gdzie są bomby. */
    }
};

class BombExploded : public Event
{
    public:

    BombId id;
    std::vector<PlayerId> robots_destroyed;

    void save_event_to_game(Game& game)
    {
        /* Usuwamy z listy zabitych graczy -  
        w nowej rozgrywce dostaniemy nową listę. */
        for (auto id: robots_destroyed)
        {
            game.players.erase(id);
        }
    }
};

class PlayerMoved : public Event
{
    public:

    PlayerId id;
    Position position;

    /* Zmiana pozycji pojedynczego gracza. */
    void save_event_to_game(Game& game)
    {
        game.players[id].position = this->position;
    }
};

class BlockPlaced : public Event
{
    public:

    Position position;
    
    /* Zapisanie położenia bloku. */
    void save_event_to_game(Game& game)
    {
        game.blocks.push_back(this->position);
    }
};

enum class ClientMessageType
{
    Join,
    PlaceBomb,
    PlaceBlock,
    Move
};

// Ten obiekt też jest usuwany po użyciu. 
class ClientMessage
{
    public:

    ClientMessageType type;
    Direction direction; // jeśli to move

    // zwraca ile bajtów zapisało 
    size_t serialize(char data[], Game& game)
    {
        if (type == ClientMessageType::Join)
        {
            size_t len = 1;
            data[0] = 0;
            len += serialize_string(&(data[1]), game.server_name);
            return len;
            
        }
        if (type == ClientMessageType::PlaceBomb)
        {
            data[0] = 1;
            return 1;
        }
        if (type == ClientMessageType::PlaceBlock)
        {
            data[0] = 2;
            return 1;
        }
        if (type == ClientMessageType::Move)
        {
            data[0] = 3;
            size_t len = 1;
            len += serialize_direction(this->direction, &(data[1]));
            return len;
        }
    }
};

class ServerMessage
{
    public:
    ServerMessage(char data[]) {} 
    
    /* Wprowadza do gry zmiany wynikające z otrzymanej wiadomości. */
    virtual void save_to_game(Game& game);
};

class Hello : public ServerMessage
{
    public:

    std::string server_name;
    uint8_t players_count;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t game_length;
    uint16_t explosion_radius;
    uint16_t bomb_timer;
    
    // konstruktor deserializujący
    Hello(char data[]) : ServerMessage(data)
    {   
    
        size_t pos = 0;
        size_t len_of_string;
        server_name = deserialize_string(&(data[pos]), len_of_string);
        pos += len_of_string;

        players_count = 
            (uint8_t)deserialize_number(&(data[pos]), sizeof(players_count));
        pos += sizeof(players_count);

        size_x = (uint16_t)deserialize_number(&(data[pos]), sizeof(size_x));
        pos += sizeof(size_x);

        size_y = (uint16_t)deserialize_number(&(data[pos]), sizeof(size_y));
        pos += sizeof(size_y);

        game_length = (uint16_t)deserialize_number(&(data[pos]), sizeof(game_length));
        pos += sizeof(game_length);

        explosion_radius = 
           (uint16_t)deserialize_number(&(data[pos]), sizeof(explosion_radius));
        pos += sizeof(explosion_radius);

        bomb_timer = 
            (uint16_t)deserialize_number(&(data[pos]), sizeof(bomb_timer));
        pos += sizeof(bomb_timer);
    }

    void save_to_game(Game& game)
    {
        game.game_state = GameStateType::Lobby;
        game.server_name = this->server_name;
        game.players_count = this->players_count;
        game.size_x = this->size_x;
        game.size_y = this->size_y;
        game.game_length = this->game_length;
        game.explosion_radius = this->explosion_radius;
        game.bomb_timer = this->bomb_timer;
    }
};

enum class InputMessageType
{
    PlaceBomb,
    PlaceBlock,
    Move
};


class InputMessage
{
    public:

    InputMessageType type;
    Direction direction; // jeśli to move

    // konstruktor z deserializacją 
    InputMessage(char data[])
    {
        if (data[0] == 0)
        {
            type = InputMessageType::PlaceBomb;
        }
        if (data[0] == 1)
        {
            type = InputMessageType::PlaceBlock;
        }
        if (data[0] == 2)
        {
            type = InputMessageType::Move;
            direction = deserialize_direction(&(data[1]));
        }
    }
};

class AcceptedPlayer : public ServerMessage
{
    public:

    PlayerId id;
    Player player;
    
    // konstruktor deserializujący
    AcceptedPlayer(char data[]) : ServerMessage(data)
    {
        
        size_t pos = 0;

        id = (PlayerId)deserialize_number(&(data[pos]), sizeof(id));
        pos += sizeof(id);

        size_t len;
        player = deserialize_player(&(data[pos]), len);
    }

    void save_to_game(Game& game)
    {
        game.players_count++; 
    }
};

class GameStarted : public ServerMessage
{
    public:

    uint32_t players_count;
    std::map<PlayerId, Player> players;
    
    // konstruktor deserializujący
    GameStarted(char data[]) : ServerMessage(data)
    {

        size_t pos = 0;

        this->players_count = 
            (uint32_t)deserialize_number(&(data[pos]), sizeof(uint32_t));
        pos += LEN_OF_SIZETYPE;

        for (size_t i = 0; i < this->players_count; i++)
        {
            PlayerId id = 
                (PlayerId)deserialize_number(&(data[pos]), sizeof(PlayerId));
            pos += sizeof(PlayerId);
            size_t size_of_player;
            Player p = deserialize_player(&(data[pos]), size_of_player);
            pos += size_of_player;
        }
    }

    void save_to_game(Game& game)
    {
        game.players = this->players;
        game.players_count = this->players_count;
    }
};

class GameEnded : public ServerMessage
{
    public:

    uint32_t players_count;
    std::map<PlayerId, Score> scores;
    
    // konstruktor deserializujący
    GameEnded(char data[]) : ServerMessage(data)
    {
        
        size_t pos = 0;

        this->players_count = 
            (uint32_t)deserialize_number(&(data[pos]), sizeof(uint32_t));
        pos += LEN_OF_SIZETYPE;

        for (size_t i = 0; i < this->players_count; i++)
        {
            PlayerId id = 
                (PlayerId)deserialize_number(&(data[pos]), sizeof(PlayerId));
            pos += sizeof(PlayerId);
            Score s = (Score)deserialize_number(&(data[pos]), sizeof(Score));
            pos += sizeof(Score);
            this->scores[id] = s;
        }
    }

    void save_to_game(Game& game)
    {
        game.scores = this->scores;
    }
};

/* Deserializuje event i wypełnia jego pozostałe pola 
(które nie są przekazane w komunikacie). */
Event deserialize_and_fill_event(char data[], size_t& read_size)
{
    PlayerMoved e;
    // nie zdążyłam tego zrobić :( 
    return e;
}

class Turn : public ServerMessage
{
    public:

    uint32_t events_count;
    std::vector<Event> events;
    
    // konstruktor deserializujący
    Turn(char data[]) : ServerMessage(data)
    {
        size_t pos = 0;

        this->events_count = 
            (uint32_t)deserialize_number(&(data[pos]), sizeof(uint32_t));
        pos += LEN_OF_SIZETYPE;

        for (size_t i = 0; i < this->events_count; i++)
        {
            size_t read_size;
            Event e = deserialize_and_fill_event(&(data[pos]), read_size);
            pos += read_size;
            this->events.push_back(e);
        }
    }

    void save_to_game(Game& game)
    {
        for (auto e: this->events)
        {
            e.save_event_to_game(game);
        }
    }
};

/* Serializuje mapę. */
size_t serialize_map(std::map<PlayerId, Player> map, char data[])
{
    // nie zdążyłam tego zrobić :(
    return 0;
}

/* Obiekt tymczasowy reprezentujący wiadomość do GUI. */
class DrawMessage
{
    virtual size_t serialize(char data[], Game& game);
};

class DrawLobby : public DrawMessage
{
    size_t serialize(char data[], Game& game)
    {
        size_t pos = 0;
        pos += serialize_string(data, game.server_name);
        
        serialize_number(
            &(data[pos]), 
            game.players_count, 
            sizeof(game.players_count));

        pos += sizeof(game.players_count);

        serialize_number(&(data[pos]), game.size_x, sizeof(game.size_x));
        pos += sizeof(game.size_x);

        serialize_number(&(data[pos]), game.size_y, sizeof(game.size_y));
        pos += sizeof(game.size_y);

        serialize_number(&(data[pos]), game.game_length, sizeof(game.game_length));
        pos += sizeof(game.game_length);

        serialize_number(
            &(data[pos]), 
            game.explosion_radius, 
            sizeof(game.explosion_radius));

        pos += sizeof(game.explosion_radius);

        serialize_number(
            &(data[pos]), 
            game.bomb_timer, 
            sizeof(game.bomb_timer));

        pos += sizeof(game.bomb_timer);

        pos += serialize_map(game.players, &(data[pos]));
        
        return pos;
    }
};

class DrawGame : public DrawMessage
{
    size_t serialize(char data[], Game& game)
    {
        size_t pos = 0;
        pos += serialize_string(data, game.server_name);

        serialize_number(&(data[pos]), game.size_x, sizeof(game.size_x));
        pos += sizeof(game.size_x);

        serialize_number(&(data[pos]), game.size_y, sizeof(game.size_y));
        pos += sizeof(game.size_y);

        serialize_number(
            &(data[pos]), 
            game.game_length, 
            sizeof(game.game_length));

        pos += sizeof(game.game_length);

        serialize_number(&(data[pos]), game.turn, sizeof(game.turn));
        pos += sizeof(game.turn);

        pos += serialize_map(game.players, &(data[pos]));
        
        return pos;
    }
};

/* Funkcja przetwarza wiadomość od serwera.
Potem czeka aż będzie potrzeba odesłania mu czegoś,
Wtedy zapisuje to w buforze data i się kończy. */
void handle_message_from_server(char data[], std::size_t& length)
{
    // nie zdążyłam tego zrobić :( 
    // ale tutaj deserializujemy data do ServerMessage
    // i wywołujemy na nim change_to_game();
    // i triggerujemy wysłanie wiadomości do gui jeśli jest taka potrzeba
    // następnie czekamy na trigger do wysłania wiadomości do serwera
    // i wtedy zapisujemy ją do data
}

/* Funkcja przetwarza wiadomość od GUI.
Potem czeka aż będzie potrzeba odesłania mu czegoś,
Wtedy zapisuje to w buforze data i się kończy. */
void handle_message_from_gui(char data[], std::size_t& length)
{
    // analogicznie jak funkcja wyżej, nie zdążyłam
}

/* Funkcja nasłuchująca w pętli komunikatów od GUI. */
void udp_server(boost::asio::io_context& io_context, std::string address,
    unsigned short port_listening, unsigned short port_sending)
{
    boost::asio::ip::udp::udp::resolver resolver_listening(io_context);
    boost::asio::ip::udp::udp::resolver resolver_sending(io_context);
    boost::asio::ip::udp::udp::endpoint endpoint_listening;
    boost::asio::ip::udp::udp::endpoint endpoint_sending;

    try 
    {
        endpoint_listening = 
            *(resolver_listening.resolve(address, std::to_string(port_listening)));
        endpoint_sending = 
            *(resolver_sending.resolve(address, std::to_string(port_sending)));
    }
    catch (std::exception& e)
    {
        
        std::cerr << "Resolving exception: wrong gui address: " << e.what() << "\n";
        exit(1);
    }
    
    
    boost::asio::ip::udp::udp::socket 
        sock_listening(io_context, endpoint_listening);
    boost::asio::ip::udp::udp::socket 
        sock_sending(io_context, endpoint_sending);

    for (;;)
    {
        char data[MAX_LENGTH];
        boost::asio::ip::udp::udp::endpoint sender_endpoint;
        size_t length = sock_listening.receive_from(
            boost::asio::buffer(data, MAX_LENGTH), sender_endpoint);
        handle_message_from_gui(data, length);
        sock_sending.send_to(boost::asio::buffer(data, length), endpoint_sending);
    }
}
/* Pojedyncze połączenie po TCP. */
void session(boost::asio::ip::tcp::tcp::socket sock)
{
    try
    {
        sock.set_option(boost::asio::ip::tcp::tcp::no_delay(true));
        for (;;)
        {
            char data[MAX_LENGTH];

            boost::system::error_code error;
            size_t length = 
                sock.read_some(boost::asio::buffer(data, MAX_LENGTH), error);
            if (error == boost::asio::error::eof)
                break; // połączenie zamknięte
            else if (error)
                throw boost::system::system_error(error);

            handle_message_from_server(data, length);
            boost::asio::write(sock, boost::asio::buffer(data, length));
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception in thread: " << e.what() << "\n";
    }
}

/* Funkcja obsługująca połączenie z serwerem. */
void tcp_server(boost::asio::io_context& io_context, std::string address,
    unsigned short port)
{
    boost::asio::ip::tcp::tcp::resolver resolver(io_context);
    boost::asio::ip::tcp::tcp::endpoint endpoint;
    try 
    {
        endpoint = *(resolver.resolve(address, std::to_string(port)));
    }
    catch (std::exception& e)
    {
        
        std::cerr << "Resolving exception: wrong server address: " << e.what() << "\n";
        exit(1);
    }

    boost::asio::ip::tcp::tcp::acceptor 
        a(io_context, boost::asio::ip::tcp::tcp::endpoint(endpoint));

    for (;;)
    {
        std::thread(session, a.accept()).detach();
    }
}

} // namespace


int main(int argc, char *argv[])
{
    std::string player_name;
    boost::program_options::options_description description("App usage");
    boost::program_options::variables_map parameter_map;
    uint16_t port_listening_gui;
    uint16_t port_sending_gui;
    uint16_t port_server;
    std::string gui_host_address;
    std::string server_host_address;
     
    description.add_options()
        ("help,h", "Get help")
        ("gui-address,d", 
            boost::program_options::value<std::string>(), "Gui address")
        ("player-name,n", 
            boost::program_options::value<std::string>(), "Player name")
        ("port,p", 
            boost::program_options::value<int64_t>(), "Port for listening gui")
        ("server-address,s", 
            boost::program_options::value<std::string>(), "Server address");

        boost::program_options::store(
            boost::program_options::command_line_parser(argc, argv).options(description).run(), 
            parameter_map);

    boost::program_options::notify(parameter_map);

    if (parameter_map.count("help"))
    {
        std::cout << description << std::endl;
        return 0;
    }

    if (parameter_map.count("gui-address"))
    {
        std::string gui_address = 
            parameter_map["gui-address"].as<std::string>();
        size_t last_colon_pos = gui_address.rfind(':');
        std::string host_part = gui_address.substr(0, last_colon_pos);
        std::string port_part = gui_address.substr(last_colon_pos + 1);
        int64_t port_sending_gui_raw = (int64_t)std::stoll(port_part);
        if (port_sending_gui_raw < 0 || port_sending_gui_raw > PORT_MAX)
        {
            std::cerr << "Wrong port value. Terminating.\n";
            return 0;
        }
        port_sending_gui = (uint16_t)port_sending_gui_raw;
        gui_host_address = host_part;
    }
    else
    {
        std::cout << "No gui address received. Terminating.\n";
        return 0;
    }

    if (parameter_map.count("player-name"))
    {
        player_name = parameter_map["player-name"].as<std::string>();
    }
    else
    {
        std::cout << "No player name received. Terminating.\n";
        return 0;
    }

    if (parameter_map.count("port"))
    {
        int64_t port_listening_gui_raw = parameter_map["port"].as<int64_t>();
        
        if (port_listening_gui_raw < 0 || port_listening_gui_raw > PORT_MAX)
        {
            std::cerr << "Wrong port value. Terminating.\n";
            return 0;
        }
        port_listening_gui = (uint16_t)port_listening_gui_raw;
    }
    else
    {
        std::cout << "No port received. Terminating.\n";
        return 0;
    }

    if (parameter_map.count("server-address"))
    {
        std::string server_address = 
            parameter_map["server-address"].as<std::string>();
        size_t last_colon_pos = server_address.rfind(':');
        std::string host_part = server_address.substr(0, last_colon_pos);
        std::string port_part = server_address.substr(last_colon_pos + 1);
        
        int64_t port_server_raw = (int64_t)std::stoll(port_part);
        if (port_server_raw < 0 || port_server_raw > PORT_MAX)
        {
            std::cerr << "Wrong port value. Terminating.\n";
            return 0;
        }
        port_server = (uint16_t)port_server_raw;

        server_host_address = host_part;
    }
    else
    {
        std::cout << "No server address received. Terminating.\n";
        return 0;
    }
   
    try
    {
        boost::asio::io_context io_context_udp;
        boost::asio::io_context io_context_tcp;

        std::thread 
            udp_thread([gui_host_address, port_listening_gui, port_sending_gui]()
            {
                boost::asio::io_context udp_io_context;
                udp_server(udp_io_context, gui_host_address, port_listening_gui, port_sending_gui);
            });
        

        std::thread 
            tcp_thread([port_server, server_host_address]() 
            {
                boost::asio::io_context io_context_tcp;
                tcp_server(io_context_tcp, server_host_address, port_server); 
            });

        udp_thread.join();
        tcp_thread.join();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
