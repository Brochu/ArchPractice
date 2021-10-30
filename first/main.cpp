#include <stdio.h>
#include <vector>
#include <algorithm>

class Subject;

class Observer
{
    public:
    Observer(Subject* pSub);
    virtual ~Observer();

    virtual void update() const = 0;

    protected:
    Subject* s;
};

class Subject
{
    public:
        void registerObserver(Observer* o)
        {
            observerCollection.push_back(o);
        }

        void unregisterObserver(Observer* o)
        {
            auto pos = std::find(observerCollection.cbegin(), observerCollection.cend(), o);
            observerCollection.erase(pos);
        }

        void notifyObservers()
        {
            for(auto* o : observerCollection)
            {
                o->update();
            }
        }

        int GetFirst() const  { return first; }
        void UpdateFirst(int newValue)
        {
            first = newValue;
            notifyObservers();
        }

        int GetSecond() const  { return second; }
        void UpdateSecond(int newValue)
        {
            second = newValue;
            notifyObservers();
        }

    private:
        int first = 0;
        int second = 0;

        std::vector<Observer*> observerCollection;
};

// THIS WOULD FAILD CODE REVIEW 100%
Observer::Observer(Subject* pSub)
{
    this->s = pSub;
    pSub->registerObserver(this);
}

Observer::~Observer()
{
    s->unregisterObserver(this);
};
// ARK ARK ARK ARK

class ConcreteObserverA : public Observer
{
    public:
    ConcreteObserverA(Subject* pSub) : Observer(pSub) {}

    virtual void update() const override
    {
        printf("[CONCRETE A] We just got updated, my value is %i\n", s->GetFirst());
    }
};

class ConcreteObserverB : public Observer
{
    public:
    ConcreteObserverB(Subject* pSub) : Observer(pSub) {}

    virtual void update() const override
    {
        printf("[CONCRETE B] We just got updated, my value is %i\n", s->GetSecond());
    }
};

int main(int argc, char** argv)
{
    printf("Hello, World!\n");

    Subject subject;
    printf("[0] We have our subject: first = %i; second = %i\n",
        subject.GetFirst(),
        subject.GetSecond()
    );

    Observer* o1 = new ConcreteObserverA(&subject);

    subject.UpdateFirst(5);
    subject.UpdateSecond(27);
    printf("[1] We have updated the subject's values with one Observer: first = %i; second = %i\n",
        subject.GetFirst(),
        subject.GetSecond()
    );

    Observer* o2 = new ConcreteObserverB(&subject);

    subject.UpdateFirst(39);
    subject.UpdateSecond(68);
    printf("[2] We have updated the subject's values with two Observer: first = %i; second = %i\n",
        subject.GetFirst(),
        subject.GetSecond()
    );

    delete o2;

    subject.UpdateFirst(42);
    subject.UpdateSecond(94);
    printf("[3] We have updated the subject's values back with one Observer: first = %i; second = %i\n",
        subject.GetFirst(),
        subject.GetSecond()
    );

    delete o1;
    return 0;
}

