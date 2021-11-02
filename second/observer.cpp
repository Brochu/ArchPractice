#include <stdio.h>
#include <vector>
#include <algorithm>

class Observer {
    public:
        virtual ~Observer() = default;

        virtual void update() = 0;
};

class Subject {
    public:
        void attach(Observer* obs)
        {
            _observers.push_back(obs);
        }

        void detach(Observer* obs)
        {
            _observers.erase(std::remove(_observers.begin(), _observers.end(), obs));
        }

        void notify()
        {
            for(auto* o : _observers)
            {
                o->update();
            }
        }

    private:
        std::vector<Observer*> _observers;
};

class MatchState : public Subject
{
    public:
        int getHomeScore() const { return _homeScore; }
        void updateHomeScore(int added)
        {
            _homeScore += added;
            notify();
        }

        int getAwayScore() const { return _awayScore; }
        void updateAwayScore(int added)
        {
            _awayScore += added;
            notify();
        }
    private:
        int _homeScore = 0;
        int _awayScore = 0;
};

class HomePanel : public Observer
{
    public:
        HomePanel(MatchState* ms)
        {
            _state = ms;
            _state->attach(this);
        }

        virtual ~HomePanel()
        {
            _state->detach(this);
        }

        virtual void update() override
        {
            printf("[HOME PANEL] New home score is %i\n", _state->getHomeScore());
        }

    private:
        MatchState* _state;
};

class AwayPanel : public Observer
{
    public:
        AwayPanel(MatchState* ms)
        {
            _state = ms;
            _state->attach(this);
        }

        virtual ~AwayPanel()
        {
            _state->detach(this);
        }

        virtual void update() override
        {
            printf("[AWAY PANEL] New away score is %i\n", _state->getAwayScore());
        }

    private:
        MatchState* _state;
};

int main(int argc, char** argv)
{
    MatchState state;

    HomePanel home(&state);
    AwayPanel away(&state);

    printf("[MAIN] Home scores first !\n");
    state.updateHomeScore(7);

    printf("[MAIN] Away retaliates !\n");
    state.updateAwayScore(3);
    printf("[MAIN] And again !\n");
    state.updateAwayScore(7);

    return 0;
}
